#pragma once

#include <memory>
#include <mutex>

#include <greyhound/defs.hpp>

namespace entwine { class Reader; }

namespace greyhound
{

class Manager;

using SharedReader = std::shared_ptr<entwine::Reader>;

class TimedReader
{
public:
    TimedReader(Manager& manager, std::string name)
        : m_manager(manager)
        , m_name(name)
        , m_touched(getNow())
    { }

    const std::string& name() const { return m_name; }
    SharedReader& get();
    std::size_t since() const { return secondsSince(m_touched); }
    std::mutex& mutex() { return m_mutex; }

    bool exists() const { return !!m_reader; }
    void reset();

private:
    void create();

    Manager& m_manager;
    std::string m_name;

    TimePoint m_touched;
    SharedReader m_reader;

    mutable std::mutex m_mutex;
};

class Resource
{
public:
    Resource(
            const Manager& manager,
            const std::string& name,
            std::vector<TimedReader*> readers);

    std::vector<TimedReader*>& readers() { return m_readers; }

    template<typename Req, typename Res> void info(Req& req, Res& res);
    template<typename Req, typename Res> void hierarchy(Req& req, Res& res);
    template<typename Req, typename Res> void files(Req& req, Res& res);
    template<typename Req, typename Res> void read(Req& req, Res& res);
    template<typename Req, typename Res> void count(Req& req, Res& res);
    template<typename Req, typename Res> void write(Req& req, Res& res);

    template<typename Req, typename Res> void infoMulti(Req& req, Res& res);
    template<typename Req, typename Res> void readMulti(Req& req, Res& res);
    template<typename Req, typename Res> void countMulti(Req& req, Res& res);
    template<typename Req, typename Res> void writeMulti(Req& req, Res& res);

private:
    const Manager& m_manager;
    const std::string m_name;
    std::vector<TimedReader*> m_readers;
};

using SharedResource = std::shared_ptr<Resource>;

} // namespace greyhound

