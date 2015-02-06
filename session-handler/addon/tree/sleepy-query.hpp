#pragma once

class LeafNode;

// TODO Duplicate code from the GreyQuery area.  Consolidate.
struct QueryIndex
{
    QueryIndex();
    QueryIndex(uint64_t id, std::size_t index) : id(id), index(index) { }
    uint64_t id;
    std::size_t index;
};

class SleepyQuery
{
public:
    SleepyQuery();

    std::size_t numPoints() const;

private:
    std::vector<std::size_t> m_baseIndexList;
    std::map<uint64_t, LeafNode*> m_overflowNodes;
    std::vector<QueryIndex> m_queryIndexList;
};

