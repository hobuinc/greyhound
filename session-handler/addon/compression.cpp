#include <stdexcept>
#include <cstring>

#include "compression.hpp"

GreyhoundStream::GreyhoundStream()
    : m_data()
    , m_index(0)
{ }

GreyhoundStream::GreyhoundStream(const std::vector<uint8_t>& data)
    : m_data(data)
    , m_index(0)
{ }

void GreyhoundStream::putBytes(const uint8_t* bytes, const std::size_t length)
{
    for (std::size_t i(0); i < length; ++i)
    {
        m_data.push_back(*bytes++);
    }
}

void GreyhoundStream::putByte(const uint8_t byte)
{
    m_data.push_back(byte);
}

uint8_t GreyhoundStream::getByte()
{
    return m_data.at(m_index++);
}

void GreyhoundStream::getBytes(uint8_t* bytes, std::size_t length)
{
    if (m_index + length > m_data.size())
    {
        throw std::runtime_error("Too many bytes requested!");
    }

    std::memcpy(bytes, m_data.data() + m_index, length);
    m_index += length;
}

const std::vector<uint8_t>& GreyhoundStream::data()
{
    return m_data;
}

