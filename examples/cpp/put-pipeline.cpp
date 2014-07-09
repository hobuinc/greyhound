// A websocket client that does a 'put' request to add a pipeline to
// Greyhound's database.
//
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <boost/algorithm/string.hpp>
#include <json/json.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>

class WebSocketClient {
    typedef websocketpp::client<websocketpp::config::asio_client> asioClient;
    typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

public:
    WebSocketClient(const std::string& uri)
        : uri(uri)
        , client()
        , jsonReader()
        , cv()
        , mutex()
    {
        using websocketpp::lib::placeholders::_1;
        using websocketpp::lib::placeholders::_2;
        using websocketpp::lib::bind;

        // We expect there to be a lot of errors, so suppress them
        client.clear_access_channels(websocketpp::log::alevel::all);
        client.clear_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO
        client.init_asio();
    }

    Json::Value exchange(Json::Value req)
    {
        Json::Value res;
        bool gotRes(false);

        std::thread t([this, &req, &res, &gotRes]()
        {
            client.set_open_handler(
                [this, &req](websocketpp::connection_hdl hdl) {
                client.send(
                        hdl,
                        req.toStyledString(),
                        websocketpp::frame::opcode::text);
            });

            client.set_message_handler(
                    [this, &res, &gotRes](
                        websocketpp::connection_hdl,
                        message_ptr msg) {
                std::unique_lock<std::mutex> lock(this->mutex);
                this->jsonReader.parse(msg->get_payload(), res);
                gotRes = true;
                lock.unlock();
                this->cv.notify_all();
            });

            websocketpp::lib::error_code ec;
            client.reset();
            client.connect(client.get_connection(uri, ec));
            client.run();
        });
        t.detach();

        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&gotRes]()->bool { return gotRes; });

        client.stop();

        return res;
    }

private:
    std::string uri;
    asioClient client;
    Json::Reader jsonReader;

    std::condition_variable cv;
    std::mutex mutex;
};

int main(int argc, char* argv[])
{
    if (argc > 2)
    {
        std::cout << "Usage: " << argv[0] <<
            " </path/to/pipeline.xml>" << std::endl;
        return 1;
    }

    WebSocketClient client("ws://localhost/");

    const std::string filename =
        argc == 2 ?
            argv[1] :
            "/vagrant/examples/data/read.xml";

    Json::Value v;
    v["command"] = "put";
    std::ifstream stream(filename, std::ios_base::in);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    v["pipeline"] = buffer.str();

    Json::Value result(client.exchange(v));

    std::cout << "Pipeline stored as ID: " <<
        result["pipelineId"].asString() << std::endl;

    return 0;
}

