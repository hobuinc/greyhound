#pragma once

#include <condition_variable>
#include <mutex>

#include <greyhound/defs.hpp>

namespace greyhound
{

class Stream
{
public:
    void putBytes(const uint8_t* bytes, std::size_t length)
    {
        m_data.insert(m_data.end(), bytes, bytes + length);
    }

    void putByte(uint8_t byte)
    {
        m_data.push_back(reinterpret_cast<const char&>(byte));
    }

    std::vector<char>& data()
    {
        return m_data;
    }

private:
    std::vector<char> m_data;
};

template<typename Res>
class Chunker
{
public:
    Chunker(Res& res, const Headers& headers)
        : m_res(res)
        , m_headers(headers)
    {
        m_data.reserve(262144 * 4);
        m_headers.emplace("Content-Type", "binary/octet-stream");
    }

    ~Chunker()
    {
        try
        {
            if (!m_done && m_headersSent) done();
        }
        catch (std::exception& e)
        {
            std::cout << "~Chunker: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cout << "~Chunker: unknown error" << std::endl;
        }
    }

    void write(bool last = false)
    {
        if (m_done) throw std::runtime_error("write was called after done");

        if (!m_headersSent)
        {
            if (last)
            {
                m_headers.emplace(
                        "Content-Length",
                        std::to_string(m_data.size()));
                m_res.write(m_headers);
                m_res.write(m_data.data(), m_data.size());
                m_done = true;
                return;
            }
            else
            {
                m_headers.emplace("Transfer-Encoding", "chunked");
                m_res.write(m_headers);
            }

            m_headersSent = true;
        }

        if (last) done();
        else if (m_data.size() > 65536)
        {
            writeChunk();
            flush();
        }
    }

    Data& data() { return m_data; }
    bool canceled() const { return m_ec == SimpleWeb::errc::broken_pipe; }
    bool cancelled() const { return canceled(); }

private:
    void done()
    {
        if (m_data.size()) writeChunk();
        m_res << "0\r\n\r\n";

        flush();
        m_done = true;
    }

    void writeChunk()
    {
        if (m_data.size())
        {
            m_res << std::hex << m_data.size() << "\r\n";
            m_res.write(m_data.data(), m_data.size());
            m_res << "\r\n";
        }
    }

    void flush()
    {
        m_res.send([this](const SimpleWeb::error_code& ec)
        {
            m_ec = ec;
            m_data.clear();
            m_cv.notify_all();
        });

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() { return m_data.empty(); });
        if (canceled()) m_done = true;
    }

    Res& m_res;
    Headers m_headers;

    Data m_data;

    SimpleWeb::error_code m_ec;
    bool m_headersSent = false;
    bool m_done = false;
    std::condition_variable m_cv;
    mutable std::mutex m_mutex;
};

} // namespace greyhound

