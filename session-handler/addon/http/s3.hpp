#pragma once

#include <string>
#include <vector>

#include "curl.hpp"

struct S3Info
{
    S3Info(
            std::string baseAwsUrl,
            std::string bucketName,
            std::string awsAccessKeyId,
            std::string awsSecretAccessKey)
        : exists(true)
        , baseAwsUrl(baseAwsUrl)
        , bucketName(bucketName)
        , awsAccessKeyId(awsAccessKeyId)
        , awsSecretAccessKey(awsSecretAccessKey)
    { }

    S3Info()
        : exists(false)
        , baseAwsUrl()
        , bucketName()
        , awsAccessKeyId()
        , awsSecretAccessKey()
    { }

    const bool exists;
    const std::string baseAwsUrl;
    const std::string bucketName;
    const std::string awsAccessKeyId;
    const std::string awsSecretAccessKey;
};

class S3
{
public:
    S3(
            std::string awsAccessKeyId,
            std::string awsSecretAccessKey,
            std::string baseAwsUrl = "s3.amazonaws.com",
            std::string bucketName = "");

    HttpResponse get(std::string file);

    HttpResponse put(std::string file, const std::vector<uint8_t>& data);
    HttpResponse put(std::string file, const std::string& data);

private:
    std::string getHttpDate() const;

    std::string getSignedEncodedString(
            std::string command,
            std::string file,
            std::string httpDate,
            std::string contentType = "") const;

    std::string getStringToSign(
            std::string command,
            std::string file,
            std::string httpDate,
            std::string contentType) const;

    std::vector<uint8_t> signString(std::string input) const;
    std::string encodeBase64(std::vector<uint8_t> input) const;

    const std::string m_awsAccessKeyId;
    const std::string m_awsSecretAccessKey;
    const std::string m_baseAwsUrl;
    const std::string m_bucketName;

    Curl m_curl;
};

