//#include <sys/stat.h>
//#include <sys/types.h>
#include <cstdio>
#include <thread>

#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/SpatialReference.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>

#include "multi-batcher.hpp"

/*
namespace
{
    std::string vfile(const std::string& pipelineId, std::size_t id)
    {
        return "/var/greyhound/tmp/" + pipelineId + "-" + std::to_string(id);
    }
}
*/

MultiBatcher::MultiBatcher(
        const S3Info& s3Info,
        const std::string& pipelineId,
        const std::size_t numBatches,
        std::shared_ptr<SleepyTree> sleepyTree)
    : m_s3(
            s3Info.awsAccessKeyId,
            s3Info.awsSecretAccessKey,
            s3Info.baseAwsUrl,
            s3Info.bucketName)
    , m_pipelineId(pipelineId)
    , m_batches(numBatches)
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
                const std::string localPath(
                        "/var/greyhound/tmp/" + m_pipelineId + "-" +
                        std::to_string(origin));

                {
                    // Retrieve remote file.
                    HttpResponse res(m_s3.get("IA_LAZlib/" + filename));

                    // TODO Retry a few times.
                    if (res.code() != 200)
                    {
                        std::cout << "Couldn't fetch " + filename <<
                                " - Got: " << res.code() << std::endl;
                        throw std::runtime_error("Couldn't fetch " + filename);
                    }

                    std::shared_ptr<std::vector<uint8_t>> fileData(res.data());
                    std::ofstream writer(
                            localPath,
                            std::ofstream::binary |
                                std::ofstream::out |
                                std::ofstream::trunc);
                    writer.write(
                            reinterpret_cast<const char*>(fileData->data()),
                            fileData->size());
                    writer.close();
                }

                /*
                // Set up virtual file writer.
                const std::string vfilename(vfile(m_pipelineId, index));
                int handle(mkfifo(vfilename.c_str(), S_IRUSR | S_IWUSR));

                if (handle < 0)
                {
                    std::cout << "Couldn't map " << filename << std::endl;
                    throw std::runtime_error("Couldn't map virtual file");
                }

                // Spawn writer process.
                std::thread writer = std::thread([handle, filename, fileData]() {
                    if (write(handle, fileData->data(), fileData->size()) < 0)
                    {
                        std::cout << "Couldn't write " << filename << std::endl;
                        throw std::runtime_error("Couldn't write virtual file");
                    }

                    std::cout << "Syncing " << filename << std::endl;
                    fsync(handle);
                });
                */

                pipelineManager->addReader(driver);

                pdal::Stage* reader(static_cast<pdal::Reader*>(
                        pipelineManager->getStage()));
                readerOptions->add(pdal::Option("filename", localPath));
                reader->setOptions(*readerOptions.get());

                /*
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
                */

                // Get and insert the buffer of reprojected points.
                pipelineManager->execute();
                const pdal::PointBufferSet& pbSet(pipelineManager->buffers());

                if (remove(localPath.c_str()) != 0)
                {
                    std::cout << "Couldn't delete " << localPath << std::endl;
                    throw std::runtime_error("Couldn't delete tmp file");
                }

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

void MultiBatcher::gather()
{
    std::thread t([this]() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool {
            return m_available.size() == m_batches.size();
        });
    });

    t.join();
}

