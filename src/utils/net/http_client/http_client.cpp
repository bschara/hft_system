#include "utils/net/http_client/http_client.h"

HttpClient::HttpClient()
{
    curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize libcurl");
    }
};

HttpClient::~HttpClient()
{
    if (curl)
    {
        curl_easy_cleanup(curl);
    }
};

std::string HttpClient::get(const std::string &url)
{
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpClient::write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        throw std::runtime_error(std::string("libcurl error: ") + curl_easy_strerror(res));
    }

    return response;
};