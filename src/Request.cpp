#include "../Request.h"
#include "RequestManager.h"
#include <curl/curl.h>
#include <mutex>
using curl_ptr = CURL *;

static Request::RequestManager<> request_manager;

Request::RequestHandler::RequestHandler(RequestHandler&& other)
: mut(other.mut), curl_num(other.curl_num), response(other.response), response_size(other.response_size), data(other.data)
{
    other.mut = nullptr;
    other.response = nullptr;
    other.response_size = 0;
    other.data = nullptr;
}

Request::RequestHandler &Request::RequestHandler::operator=(RequestHandler &&other)
{
    join();
    mut = other.mut;
    response = other.response;
    response_size = other.response_size;
    data = other.data;

    other.mut = nullptr;
    other.response = nullptr;
    other.response_size = 0;
    other.data = nullptr;
    return *this;
}

size_t Request::RequestHandler::join()
{
    if (response)
    {
        auto started = startAllRequests(0);
        reinterpret_cast<std::mutex *>(mut)->lock();
        if (data)
        {
            delete data;
            data = nullptr;
        }
        response = nullptr;
        reinterpret_cast<std::mutex *>(mut)->unlock();
        delete reinterpret_cast<std::mutex *>(mut);
        if (started)
        {
            stopAllRequests();
        }
    }
    return response_size;
}

bool Request::RequestHandler::done()
{
    if (reinterpret_cast<std::mutex *>(mut)->try_lock())
    {
        reinterpret_cast<std::mutex *>(mut)->unlock();
        return true;
    }
    return false;
}

Request::RequestHandler::~RequestHandler()
{
    join();
}

size_t callBack(char *ptr, size_t size, size_t n_memb, void *userdata)
{
    size_t real_size = size * n_memb;
    auto &true_userdata = *reinterpret_cast<Request::RequestHandler *>(userdata);
    auto &dat = *reinterpret_cast<char **>(true_userdata.response);
    dat = reinterpret_cast<char *>(std::realloc(dat, true_userdata.response_size + real_size));

    if (!dat)
    {
        return 0;
    }
    auto st = dat + true_userdata.response_size;
    for (size_t k = 0; k < real_size; ++k)
    {
        st[k] = ptr[k];
    }
    true_userdata.response_size += real_size;
    return real_size;
}

size_t callBackFile(char *ptr, size_t size, size_t n_memb, void *userdata)
{
    size_t real_size = size * n_memb;
    auto &true_userdata = *reinterpret_cast<Request::RequestHandler *>(userdata);
    auto &dat = *reinterpret_cast<FILE **>(true_userdata.response);
    fwrite(ptr, size, n_memb, dat);
    true_userdata.response_size += real_size;
    return real_size;
}

char* readFile(FILE* f)
{
    int off_set = ftell(f);
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, off_set, SEEK_SET);
    size_t buf_size = file_size - off_set;
    char *buf = new char[buf_size + 1];
    fread(buf, sizeof(char), buf_size, f);
    buf[buf_size] = 0;
    return buf;
}

Request::void_ptr Request::generateHeader(const char **h, const size_t &h_num)
{
    curl_slist *headers = nullptr;
    for (size_t k = 0; k < h_num; ++k)
    {
        headers = curl_slist_append(headers, h[k]);
    }
    return reinterpret_cast<void *>(headers);
}

void Request::freeHeader(void *h)
{
    curl_slist_free_all(reinterpret_cast<curl_slist *>(h));
}

void Request::addRequest(RequestHandler& handler, const char* url, void_ptr& output, const bool &is_file_output, const void_ptr &h, const RequestType &rt, const void_ptr &data, const bool &is_file_data)
{
    //init curl pointer
    CURL *curl_e;
    if (request_manager.easy_pointers.available() == 0)
    {
        request_manager.easy_pointers.add(curl_easy_init());
    }
    auto id = request_manager.easy_pointers.getNextResource(&curl_e);

    //add url
    curl_easy_setopt(curl_e, CURLOPT_URL, url);
    
    //set up callback function
    if (is_file_output)
    {
        curl_easy_setopt(curl_e, CURLOPT_WRITEFUNCTION, callBackFile);
    }
    else
    {
        curl_easy_setopt(curl_e, CURLOPT_WRITEFUNCTION, callBack);
    }
    curl_easy_setopt(curl_e, CURLOPT_WRITEDATA, reinterpret_cast<void_ptr>(&handler));
    
    //add header
    if (h)
    {
        curl_easy_setopt(curl_e, CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(h));
    }

    //add data to send
    if (data)
    {
        if (is_file_data)
        {
            if (rt != RequestType::POST && rt != RequestType::PUT)
            {
                handler.data = readFile(reinterpret_cast<FILE *>(data));
                curl_easy_setopt(curl_e, CURLOPT_POSTFIELDS, handler.data);
            }
            else
            {
                curl_easy_setopt(curl_e, CURLOPT_READFUNCTION, nullptr);
                curl_easy_setopt(curl_e, CURLOPT_READDATA, data);
            }
        }
        else
        {
            curl_easy_setopt(curl_e, CURLOPT_POSTFIELDS, data);
        }
    }

    //request type
    switch (rt)
    {
    case RequestType::POST:
        curl_easy_setopt(curl_e, CURLOPT_POST, 1L);
        break;
    case RequestType::PUT:
        curl_easy_setopt(curl_e, CURLOPT_UPLOAD, 1L);
        break;
    case RequestType::DELETE:
        curl_easy_setopt(curl_e, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case RequestType::GET:
        curl_easy_setopt(curl_e, CURLOPT_CUSTOMREQUEST, "GET");
        break;
    }
    auto &multi_pointer = request_manager.multi_pointer;

    //handler init
    curl_multi_add_handle(multi_pointer, curl_e);
    handler.mut = new std::mutex();
    reinterpret_cast<std::mutex *>(handler.mut)->lock();
    handler.curl_num = id;
    handler.response = &output;

    request_manager.mut_mem_var.lock();
    request_manager.fresh_add = true;
    request_manager.curl_pointer_map[curl_e] = &handler;
    request_manager.mut_mem_var.unlock();
}

bool Request::startAllRequests(const size_t& timeout)
{
    auto f = [timeout]()
    {
        static size_t num_ran = 0;
        auto &running = request_manager.running;
        do
        {
            downloadIfPossible(timeout);
        } while (running);
        request_manager.mut_main_thread.unlock();
    };
    if (request_manager.main_thread)
    {
        if (request_manager.mut_main_thread.try_lock())
        {
            request_manager.main_thread->join();
            request_manager.mut_main_thread.lock();
            *request_manager.main_thread = std::thread(f);
            request_manager.mut_main_thread.unlock();
            return true;
        }
    }
    else
    {
        request_manager.mut_main_thread.lock();
        request_manager.main_thread = new std::thread(f);
        return true;
    }
    return false;
}

void Request::downloadIfPossible(const size_t& timeout)
{
    static int last = 0;
    auto &multi_pointer = request_manager.multi_pointer;
    auto &mut_mem_var = request_manager.mut_mem_var;
    auto &curl_pointer_map = request_manager.curl_pointer_map;
    auto &easy_pointers = request_manager.easy_pointers;
    auto &running = request_manager.running;
    auto &fresh_add = request_manager.fresh_add;
    int num_fd;
    mut_mem_var.lock();
    curl_multi_wait(multi_pointer, nullptr, 0, timeout, &num_fd);
    curl_multi_perform(multi_pointer, &running);
    if (last != running || fresh_add)
    {
        int in_q = 0;
        CURLMsg *mes;
        while (mes = curl_multi_info_read(multi_pointer, &in_q))
        {
            if (mes->msg == CURLMSG_DONE)
            {
                auto e_pointer = mes->easy_handle;
                auto hand = curl_pointer_map[e_pointer];
                easy_pointers.returnResource(hand->curl_num);
                curl_multi_remove_handle(multi_pointer, e_pointer);
                reinterpret_cast<std::mutex*>(hand->mut)->unlock();
            }
        }
    }
    last = running;
    fresh_add = false;
    mut_mem_var.unlock();
}

void Request::stopAllRequests()
{
    request_manager.mut_mem_var.lock();
    auto temp = request_manager.running;
    request_manager.running = 0;
    request_manager.mut_mem_var.unlock();
    request_manager.main_thread->join();
    request_manager.running = temp;
}

char *Request::urlEncode(const char *const &str, const size_t &len)
{
    CURL *curl = curl_easy_init();
    char *result = nullptr;
    if (curl)
    {
        result = curl_easy_escape(curl, str, len);
    }
    curl_easy_cleanup(curl);
    return result;
}

void Request::freeEncode(char *const &str)
{
    curl_free(str);
}