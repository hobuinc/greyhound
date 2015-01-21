#pragma once

#include <vector>
#include <string>

#include <curl/curl.h>

class HttpResponse
{
public:
    HttpResponse(int code)
        : m_code(code)
        , m_data(data)
    { }

    HttpResponse(int code, std::vector<uint8_t> data)
        : m_code(code)
        , m_data(data)
    { }

    HttpResponse(const HttpResponse& other)
        : m_code(other.m_code)
        , m_data(other.m_data)
    { }

    ~HttpResponse() { }

    int code() const { return m_code; }
    const std::vector<uint8_t>& data() const { return m_data; }

private:
    int m_code;
    std::vector<uint8_t> m_data;
};

class Curl
{
public:
    Curl(std::string url, bool followRedirect = true, bool verbose = false);
    ~Curl();

    void addHeaders(std::vector<std::string> headers);
    void addHeader(std::string header);

    HttpResponse get();
    HttpResponse put(const std::vector<uint8_t>& data);

private:
    CURL* m_curl;
    curl_slist* m_headers;

    std::vector<uint8_t> m_data;
};

