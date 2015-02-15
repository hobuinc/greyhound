#include <chrono>
#include <fstream>
#include <limits>
#include <thread>

#include <pdal/Charbuf.hpp>
#include <pdal/Compression.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/QuadIndex.hpp>

#include "compression-stream.hpp"
#include "node.hpp"

namespace
{
    // Factory method used during construction.
    Node* create(
            std::vector<char>* basePoints,
            std::size_t pointSize,
            const BBox& bbox,
            std::size_t overflowDepth,
            std::size_t depth,
            uint64_t id)
    {
        if (depth < overflowDepth)
        {
            return new StemNode(
                    basePoints,
                    pointSize,
                    bbox,
                    overflowDepth,
                    depth,
                    id);
        }
        else if (depth == overflowDepth)
        {
            return new LeafNode(bbox, id);
        }
        else
        {
            throw std::runtime_error("Invalid factory parameters!");
        }
    }

    // TODO
    const std::string diskPath("/var/greyhound/serial");
}

StemNode::StemNode(
        std::vector<char>* base,
        const std::size_t pointSize,
        const BBox& bbox,
        std::size_t overflow,
        std::size_t depth,
        const uint64_t id)
    : Node(bbox, id)
    , point(0)
    , offset(base->size())
{
    base->resize(base->size() + pointSize);
    nw.reset(create(
                base,
                pointSize,
                bbox.getNw(),
                overflow,
                depth + 1,
                (id << 2) | nwFlag));
    ne.reset(create(
                base,
                pointSize,
                bbox.getNe(),
                overflow,
                depth + 1,
                (id << 2) | neFlag));
    sw.reset(create(
                base,
                pointSize,
                bbox.getSw(),
                overflow,
                depth + 1,
                (id << 2) | swFlag));
    se.reset(create(
                base,
                pointSize,
                bbox.getSe(),
                overflow,
                depth + 1,
                (id << 2) | seFlag));
}

StemNode::~StemNode()
{
    if (point.load()) delete point.load();
}

LeafNode* StemNode::addPoint(std::vector<char>* base, PointInfo** toAddPtr)
{
    PointInfo* toAdd(*toAddPtr);
    if (point.load())
    {
        const Point mid(bbox.mid());
        if (toAdd->point->sqDist(mid) < point.load()->sqDist(mid))
        {
            std::lock_guard<std::mutex> lock(mutex);
            const Point* curPoint(point.load());

            // Reload the reference point now that we've acquired the lock.
            if (toAdd->point->sqDist(mid) < curPoint->sqDist(mid))
            {
                // Pull out the old stored value and store the heap-allocated
                // Point that was in our atomic member, so we can overwrite
                // that with the new one.
                PointInfo* old(
                        new PointInfo(
                            curPoint,
                            base->data() + offset,
                            toAdd->bytes.size()));

                // Store this point in the base data store.
                toAdd->write(base->data() + offset);
                point.store(toAdd->point);
                delete toAdd;

                // Send our old stored value downstream.
                toAdd = old;
            }
        }

        if (toAdd->point->x < mid.x)
        {
            if (toAdd->point->y < mid.y)
            {
                return sw->addPoint(base, &toAdd);
            }
            else
            {
                return nw->addPoint(base, &toAdd);
            }
        }
        else
        {
            if (toAdd->point->y < mid.y)
            {
                return se->addPoint(base, &toAdd);
            }
            else
            {
                return ne->addPoint(base, &toAdd);
            }
        }
    }
    else
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!point.load())
        {
            toAdd->write(base->data() + offset);
            point.store(toAdd->point);
            delete toAdd;
        }
        else
        {
            // Someone beat us here, call again to enter the other branch.
            // Be sure to unlock our mutex first.
            lock.unlock();
            return addPoint(base, &toAdd);
        }
    }

    return 0;
}

void StemNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t curDepth)
{
    if (point.load())
    {
        if (curDepth >= depthBegin && (curDepth < depthEnd || !depthEnd))
        {
            results.push_back(
                    std::make_pair(0, offset / pointContext.pointSize()));
        }

        if (++curDepth < depthEnd || !depthEnd)
        {
            nw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
            ne->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
            se->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
            sw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
        }
    }
}

void StemNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        const BBox& query,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t depth)
{
    const Point* p(point.load());
    if (p && query.overlaps(bbox))
    {
        if (
                query.contains(*p) &&
                depth >= depthBegin &&
                (depth < depthEnd || !depthEnd))
        {
            results.push_back(
                    std::make_pair(0, offset / pointContext.pointSize()));
        }

        if (++depth < depthEnd || !depthEnd)
        {
            nw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
            ne->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
            se->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
            sw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
        }
    }
}

LeafNode::LeafNode(const BBox& bbox, uint64_t id)
    : Node(bbox, id)
    , overflow()
    , quadIndex()
{ }

LeafNode::~LeafNode()
{ }

LeafNode* LeafNode::addPoint(std::vector<char>* base, PointInfo** toAddPtr)
{
    PointInfo* toAdd(*toAddPtr);

    std::lock_guard<std::mutex> lock(mutex);
    if (!overflow)
    {
        overflow.reset(new std::vector<char>());
    }

    overflow->insert(overflow->end(), toAdd->bytes.begin(), toAdd->bytes.end());

    delete toAdd->point;
    delete toAdd;

    return this;
}

void LeafNode::save(
        const std::string& pipelineId,
        const pdal::PointContextRef pointContext)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!overflow) return;

    const std::string path(
            diskPath + "/" + pipelineId + "/" + std::to_string(id));

    std::ofstream dataStream;
    dataStream.open(
            path,
            std::ofstream::out | std::ofstream::app | std::ofstream::binary);
    if (!dataStream.good()) return;

    const uint64_t uncompressedSize(overflow->size());
    std::shared_ptr<std::vector<char>> compressed(
            compress(*overflow.get(), pointContext));
    const uint64_t compressedSize(compressed->size());

    dataStream.write(
            reinterpret_cast<const char*>(&uncompressedSize),
            sizeof(uint64_t));
    dataStream.write(
            reinterpret_cast<const char*>(&compressedSize),
            sizeof(uint64_t));
    dataStream.write(compressed->data(), compressed->size());
    dataStream.close();

    overflow.reset();
}

std::shared_ptr<std::vector<char>> LeafNode::compress(
        std::vector<char>& uncompressed,
        const pdal::PointContextRef pointContext) const
{
    CompressionStream compressionStream;
    pdal::LazPerfCompressor<CompressionStream> compressor(
            compressionStream,
            pointContext.dimTypes());

    compressor.compress(uncompressed.data(), uncompressed.size());
    compressor.done();

    std::shared_ptr<std::vector<char>> compressed(
            new std::vector<char>(compressionStream.data().size()));

    std::memcpy(
            compressed->data(),
            compressionStream.data().data(),
            compressed->size());

    return compressed;
}

std::shared_ptr<std::vector<char>> LeafNode::decompress(
        const std::vector<char>& compressed,
        const std::size_t uncompressedSize,
        const pdal::PointContextRef pointContext) const
{
    CompressionStream compressionStream(compressed);

    pdal::LazPerfDecompressor<CompressionStream> decompressor(
            compressionStream,
            pointContext.dimTypes());

    std::shared_ptr<std::vector<char>> uncompressed(
            new std::vector<char>(uncompressedSize));

    decompressor.decompress(uncompressed->data(), uncompressedSize);

    return uncompressed;
}

void LeafNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        std::size_t depthBegin,
        std::size_t depthEnd,
        std::size_t curDepth)
{
    if (!build(pointContext, pipelineId, curDepth))
    {
        return;
    }

    quadIndex->getPoints(depthBegin, depthEnd);

    const std::size_t end(overflow->size() / pointContext.pointSize());
    for (std::size_t i(0); i < end; ++i)
    {
        results.push_back(std::make_pair(id, i));
    }

    cache.insert(id, overflow);
}

void LeafNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        const BBox& query,
        std::size_t depthBegin,
        std::size_t depthEnd,
        std::size_t curDepth)
{
    if (
            !query.overlaps(bbox) ||
            !build(pointContext, pipelineId, curDepth))
    {
        return;
    }

    std::vector<std::size_t> indexList(quadIndex->getPoints(
            query.min().x,
            query.min().y,
            query.max().x,
            query.max().y,
            depthBegin,
            depthEnd));

    for (std::size_t i(0); i < indexList.size(); ++i)
    {
        results.push_back(std::make_pair(id, indexList[i]));
    }

    cache.insert(id, overflow);
}

bool LeafNode::build(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        std::size_t curDepth)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (overflow) return true;

    const std::string path(
            diskPath + "/" + pipelineId + "/" + std::to_string(id));

    std::ifstream dataStream;
    dataStream.open(path, std::ifstream::in | std::ifstream::binary);
    if (!dataStream.good()) return false;

    uint64_t uncompressedSize(0);
    uint64_t compressedSize(0);
    overflow.reset(new std::vector<char>());

    do
    {
        // We need to read past the end to get the eof bit set, if applicable.
        dataStream.read(
                reinterpret_cast<char*>(&uncompressedSize),
                sizeof(uint64_t));

        if (!dataStream.eof())
        {
            dataStream.read(
                    reinterpret_cast<char*>(&compressedSize),
                    sizeof(uint64_t));

            std::vector<char> compressed(compressedSize);
            dataStream.read(compressed.data(), compressed.size());

            std::shared_ptr<std::vector<char>> points(
                    decompress(
                        compressed,
                        uncompressedSize,
                        pointContext));

            std::cout << "    Inserting " << points->size() << std::endl;
            overflow->insert(overflow->end(), points->begin(), points->end());
        }
    } while (!dataStream.eof());

    dataStream.close();
    std::cout << "File read" << std::endl;

    const std::size_t pointSize(pointContext.pointSize());
    const std::size_t numPoints(overflow->size() / pointSize);
    std::vector<std::shared_ptr<pdal::QuadPointRef>> pointList(numPoints);

    double x(0);
    double y(0);

    for (std::size_t i(0); i < numPoints; ++i)
    {
        std::memcpy(
                &x,
                overflow->data() + i * pointSize,
                sizeof(double));
        std::memcpy(
                &y,
                overflow->data() + i * pointSize + sizeof(double),
                sizeof(double));

        pointList[i].reset(new pdal::QuadPointRef(pdal::Point(x, y), i));
    }

    quadIndex.reset(
            new pdal::QuadIndex(
                pointList,
                bbox.min().x,
                bbox.min().y,
                bbox.max().x,
                bbox.max().y,
                curDepth));

    return true;
}

