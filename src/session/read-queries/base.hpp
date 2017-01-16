#pragma once

#include <memory>
#include <vector>

#include <pdal/Dimension.hpp>
#include <pdal/Compression.hpp>

#include <entwine/util/compression.hpp>

namespace entwine
{
    class Schema;
    class DimInfo;
}

class ReadQuery
{
public:
    ReadQuery(
            const entwine::Schema& schema,
            bool compress,
            std::size_t index = 0);
    virtual ~ReadQuery() { if (m_compressor) m_compressor->done(); }

    void read(std::vector<char>& buffer);
    bool compress() const { return m_compressor.get() != 0; }
    bool done() const { return m_done; }
    virtual uint64_t numPoints() const = 0;

protected:
    // Must return true if done, else false.
    virtual bool readSome(std::vector<char>& buffer) = 0;

    entwine::CompressionStream m_compressionStream;
    std::unique_ptr<pdal::LazPerfCompressor<
            entwine::CompressionStream>> m_compressor;
    std::size_t m_compressionOffset;

    const entwine::Schema& m_schema;
    bool m_done;
};

