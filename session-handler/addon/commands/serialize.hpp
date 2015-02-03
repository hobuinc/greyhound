
struct SerializeData : public Background
{
    SerializeData(
            std::shared_ptr<PdalSession> pdalSession,
            SerialPaths paths,
            v8::Persistent<v8::Function> callback)
        : pdalSession(pdalSession)
        , paths(paths)
        , callback(callback)
    { }

    ~SerializeData()
    {
        callback.Dispose();
    }

    // Inputs
    const std::shared_ptr<PdalSession> pdalSession;
    const SerialPaths paths;

    // Outputs
    std::string errMsg;

    v8::Persistent<v8::Function> callback;
};

