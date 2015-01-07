#pragma once

#include <cstdint>
#include <vector>

class GreyhoundStream
{
public:
    GreyhoundStream();
    GreyhoundStream(const std::vector<uint8_t>& data);

    void putBytes(const uint8_t* bytes, std::size_t length);
    void putByte(uint8_t byte);

    uint8_t getByte();
    void getBytes(uint8_t* bytes, std::size_t length);

    const std::vector<uint8_t>& data();

private:
    std::vector<uint8_t> m_data;
    std::size_t m_index;
};

