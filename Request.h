#pragma once
#include <mutex>

namespace Request
{
    using void_ptr = void *;
    enum class RequestType
    {
        GET = 0,
        POST = 1,
        PUT = 2,
        DELETE = 3
    };

    struct RequestHandler
    {
        void_ptr mut;
        size_t curl_num;
        void_ptr response = nullptr;
        size_t response_size = 0;
        char *data = nullptr;
        size_t join();
        ~RequestHandler();
    };

    void addRequest(RequestHandler &handler, const char *url, void_ptr& output, const bool &is_file_output = true, const void_ptr &h = nullptr, const RequestType &rt = RequestType::GET, const void_ptr &data = nullptr, const bool& is_file_data = true);
    bool startAllRequests(const size_t& timeout);
    void downloadIfPossible(const size_t &timeout);
    void_ptr generateHeader(const char **h, const size_t &h_num);
    void freeHeader(void *h);
    void stopAllRequests();
}
