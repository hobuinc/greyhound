#pragma once

#include <string>
#include <vector>

#include "curl.hpp"

class S3
{
public:
    S3(
            std::string awsAccessKeyId,
            std::string awsSecretAccessKey,
            std::string baseAwsUrl = "s3.amazonaws.com",
            std::string bucketName = "");

    HttpResponse get(
            std::string file,
            bool verbose = false);

    HttpResponse put(
            std::string file,
            const std::vector<uint8_t>& data,
            bool verbose = false);

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
};

