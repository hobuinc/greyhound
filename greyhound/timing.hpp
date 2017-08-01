#pragma once

namespace greyhound
{

inline TimePoint getNow()
{
    return std::chrono::high_resolution_clock::now();
}

inline std::size_t secondsSince(TimePoint start)
{
    std::chrono::duration<double> d(getNow() - start);
    return std::chrono::duration_cast<std::chrono::seconds>(d).count();
}

inline std::size_t secondsBetween(TimePoint start, TimePoint end)
{
    std::chrono::duration<double> d(end - start);
    return std::chrono::duration_cast<std::chrono::seconds>(d).count();
}

} // namespace greyhound

