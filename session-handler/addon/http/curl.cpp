#include <cstring>
#include <iostream>

#include "curl.hpp"

namespace
{
    struct PutData
    {
        PutData(const std::vector<uint8_t>& data)
            : data(data)
            , offset(0)
        { }

        std::vector<uint8_t> data;
        std::size_t offset;
    };

    std::size_t getCb(
            const uint8_t* in,
            std::size_t size,
            std::size_t num,
            std::vector<uint8_t>* out)
    {
        const std::size_t fullBytes(size * num);
        const std::size_t startSize(out->size());

        out->resize(out->size() + fullBytes);
        std::memcpy(out->data() + startSize, in, fullBytes);

        return fullBytes;
    }

    std::size_t putCb(
            uint8_t* out,
            std::size_t size,
            std::size_t num,
            PutData* in)
    {
        const std::size_t fullBytes(
                std::min(
                    size * num,
                    in->data.size() - in->offset));
        std::memcpy(out, in->data.data() + in->offset, fullBytes);

        in->offset += fullBytes;
        return fullBytes;
    }
}

Curl::Curl(std::string url, bool followRedirect, bool verbose)
    : m_curl(0)
    , m_headers(0)
    , m_data()
{
    m_curl = curl_easy_init();
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

    if (verbose)
        curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
    if (followRedirect)
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Needed for multithreaded Curl usage.
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);

    // Faster (by a lot) DNS lookups without IPv6.
    curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
}

Curl::~Curl()
{
    curl_easy_cleanup(m_curl);
}

void Curl::addHeaders(std::vector<std::string> headers)
{
    for (std::size_t i(0); i < headers.size(); ++i)
    {
        addHeader(headers[i]);
    }
}

void Curl::addHeader(std::string header)
{
    // Add to our list of headers, which won't be added to the request yet.
    m_headers = curl_slist_append(m_headers, header.c_str());
}

HttpResponse Curl::get()
{
    int httpCode(0);
    std::vector<uint8_t>* data = new std::vector<uint8_t>();

    // Register callback function and date pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, data);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    HttpResponse res(httpCode, *data);
    delete data;
    return res;
}

HttpResponse Curl::put(const std::vector<uint8_t>& data)
{
    int httpCode(0);

    PutData* putData(new PutData(data));

    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, putData);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Specify that this is a PUT request.
    curl_easy_setopt(m_curl, CURLOPT_PUT, 1L);

    // Must use this for binary data, otherwise curl will use  strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(m_curl, CURLOPT_INFILESIZE_LARGE, data.size());

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    return HttpResponse(httpCode);
}

