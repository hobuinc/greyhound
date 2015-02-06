#include "tree/sleepy-query.hpp"

SleepyQuery::SleepyQuery()
{
    // TODO
}

std::size_t SleepyQuery::numPoints() const
{
    return m_baseIndexList.size() + m_queryIndexList.size();
}


