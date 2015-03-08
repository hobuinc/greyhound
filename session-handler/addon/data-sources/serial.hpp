#include "grey/reader.hpp"
#include "base.hpp"

namespace pdal
{
    class PointContext;
}

namespace entwine
{
    class BBox;
    class Schema;
}

class RasterMeta;
class SerialPaths;

class SerialDataSource : public DataSource
{
public:
    SerialDataSource(
            const std::string& pipelineId,
            const SerialPaths& serialPaths);
    SerialDataSource(GreyReader* greyReader);
    ~SerialDataSource() { }

    const pdal::PointContext& pointContext() const;

    virtual std::size_t getNumPoints() const;
    virtual std::string getSchema() const;
    virtual std::string getStats() const;
    virtual std::string getSrs() const;
    virtual std::vector<std::size_t> getFills() const;

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compressed,
            const entwine::BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compressed,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compressed,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

private:
    std::unique_ptr<GreyReader> m_greyReader;
};

