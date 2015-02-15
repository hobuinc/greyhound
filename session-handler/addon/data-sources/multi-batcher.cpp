#include <thread>
#include <forward_list>

#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/SpatialReference.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>

#include "multi-batcher.hpp"

MultiBatcher::MultiBatcher(
        const std::size_t numBatches,
        std::shared_ptr<SleepyTree> sleepyTree)
    : m_batches(numBatches)
    , m_available(numBatches)
    , m_sleepyTree(sleepyTree)
    , m_mutex()
    , m_cv()
{
    for (std::size_t i(0); i < m_available.size(); ++i)
    {
        m_available[i] = i;
    }
}

void MultiBatcher::add(const std::string& filename, const Origin origin)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return m_available.size(); });
    const std::size_t index(m_available.back());
    m_available.pop_back();
    std::cout << "Adding " << filename << std::endl;
    lock.unlock();

    m_batches[index] = std::thread([this, index, &filename, origin]() {
        try
        {
            std::unique_ptr<pdal::PipelineManager> pipelineManager(
                    new pdal::PipelineManager());
            std::unique_ptr<pdal::StageFactory> stageFactory(
                    new pdal::StageFactory());
            std::unique_ptr<pdal::Options> readerOptions(
                    new pdal::Options());

            const std::string driver(stageFactory->inferReaderDriver(filename));
            if (driver.size())
            {
                pipelineManager->addReader(driver);

                pdal::Stage* reader(static_cast<pdal::Reader*>(
                        pipelineManager->getStage()));
                readerOptions->add(pdal::Option("filename", filename));
                reader->setOptions(*readerOptions.get());

                reader->setSpatialReference(
                        pdal::SpatialReference("EPSG:26915"));

                // Reproject to Web Mercator.
                pipelineManager->addFilter(
                        "filters.reprojection",
                        pipelineManager->getStage());

                pdal::Options srsOptions;
                srsOptions.add(
                        pdal::Option(
                            "in_srs",
                            pdal::SpatialReference("EPSG:26915")));
                srsOptions.add(
                        pdal::Option(
                            "out_srs",
                            pdal::SpatialReference("EPSG:3857")));

                pipelineManager->getStage()->setOptions(srsOptions);

                // Get and insert the buffer of reprojected points.
                pipelineManager->execute();
                const pdal::PointBufferSet& pbSet(pipelineManager->buffers());

                for (const auto pointBuffer : pbSet)
                {
                    m_sleepyTree->insert(pointBuffer.get(), origin);
                }
            }
            else
            {
                std::cout << "No driver found - " << filename << std::endl;
            }
        }
        catch (...)
        {
            std::cout << "Exception in multi-batcher " << filename << std::endl;
        }

        std::cout << "    Done " << filename << std::endl;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_available.push_back(index);
        m_batches[index].detach();
        lock.unlock();
        m_cv.notify_all();
    });
}

void MultiBatcher::done()
{
    std::thread t([this]() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool {
            return m_available.size() == m_batches.size();
        });
    });

    t.join();

    m_sleepyTree->save();
}

