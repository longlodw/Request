#pragma once
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <vector>
#include <map>

namespace Request
{
    enum class RequestType
    {
        GET,
        POST,
        PUT,
        DELETE
    };

    struct RequestResult
    {    
        std::mutex mut;
        size_t curl_num;
        void *manager;
        char *data = nullptr;
        size_t data_size = 0;

        char* getData();
        size_t getSize();
        ~RequestResult();
    };

    void addRequest(RequestResult& result, const char *url, const char** h = nullptr, const size_t& h_num = 0, const RequestType& rt = RequestType::GET);
    void startAllRequests();
}

