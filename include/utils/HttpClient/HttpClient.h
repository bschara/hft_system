#pragma once

#include <curl/curl.h>
#include <string>
#include <iostream>

class HttpClient
{
public:
    HttpClient();

    ~HttpClient();

    std::string get(const std::string &url);

private:
    CURL *curl;

    static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        size_t totalSize = size * nmemb;
        std::string *response = static_cast<std::string *>(userp);
        response->append(static_cast<char *>(contents), totalSize);
        return totalSize;
    }
};
