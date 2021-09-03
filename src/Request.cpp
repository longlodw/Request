#include "Request.h"
#include "RequestManager.h"
using namespace Request;
static RequestManager<> request_manager;

char* RequestResult::getData()
{
    mut.lock();
    char *re = data;
    data = nullptr;
    mut.unlock();
    return re;
}

RequestResult::~RequestResult()
{
    if (!data)
    {
        delete[] data;
    }
    mut.try_lock();
    mut.unlock();
}

size_t callBack(char *ptr, size_t size, size_t n_memb, void *userdata)
{
    size_t real_size = size * n_memb;
    auto &dat = reinterpret_cast<RequestResult *>(userdata)->data;
    dat = reinterpret_cast<char *>(std::realloc(dat, reinterpret_cast<RequestResult *>(userdata)->data_size + real_size));

    if (!dat)
    {
        return size_t(0);
    }
    auto st = dat + reinterpret_cast<RequestResult *>(userdata)->data_size;
    for (size_t k = 0; k < real_size; ++k)
    {
        st[k] = ptr[k];
    }
    reinterpret_cast<RequestResult *>(userdata)->data_size += real_size;
    return real_size;
}

void Request::addRequest(RequestResult& result, const char* url, const char** h, const size_t& h_num, const RequestType& rt)
{
    CURL *curl_e;
    if (request_manager.easy_pointers.available() == 0)
    {
        request_manager.easy_pointers.add(curl_easy_init());
    }
    auto id = request_manager.easy_pointers.getNextResource(&curl_e);
    curl_easy_setopt(curl_e, CURLOPT_URL, url);
    
    curl_easy_setopt(curl_e, CURLOPT_WRITEFUNCTION, callBack);
    curl_easy_setopt(curl_e, CURLOPT_WRITEDATA, reinterpret_cast<void *>(&result));
    curl_slist *headers = nullptr;
    for (size_t k = 0; k < h_num; ++k)
    {
        headers = curl_slist_append(headers, h[k]);
    }
    if (headers)
    {
        curl_easy_setopt(curl_e, CURLOPT_HTTPHEADER, headers);
    }
    auto &multi_pointer = request_manager.multi_pointer;
    switch (rt)
    {
    case RequestType::GET:
        curl_easy_setopt(multi_pointer, CURLOPT_CUSTOMREQUEST, "GET");
        break;
    case RequestType::POST:
        curl_easy_setopt(multi_pointer, CURLOPT_CUSTOMREQUEST, "POST");
        break;
    case RequestType::PUT:
        curl_easy_setopt(multi_pointer, CURLOPT_CUSTOMREQUEST, "PUT");
        break;
    case RequestType::DELETE:
        curl_easy_setopt(multi_pointer, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    default:
        break;
    }
    curl_multi_add_handle(multi_pointer, curl_e);
    result.mut.lock();
    result.curl_num = id;
    result.manager = &request_manager;
    
    request_manager.mut_mem_var.lock();
    request_manager.fresh_add = true;
    request_manager.curl_pointer_map[curl_e] = &result;
    request_manager.mut_mem_var.unlock();
}

void Request::startAllRequests()
{
    static auto f = []()
    {
        static int last;
        request_manager.mut_main_thread.lock();
        auto &running = request_manager.running;
        auto &multi_pointer = request_manager.multi_pointer;
        auto &fresh_add = request_manager.fresh_add;
        auto &mut_mem_var = request_manager.mut_mem_var;
        auto &curl_pointer_map = request_manager.curl_pointer_map;
        auto &easy_pointers = request_manager.easy_pointers;
        auto &mut_main_thread = request_manager.mut_main_thread;
        do
        {
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
                        mut_mem_var.lock();
                        auto re = curl_pointer_map[e_pointer];
                        easy_pointers.returnResource(re->curl_num);
                        mut_mem_var.unlock();
                        curl_multi_remove_handle(multi_pointer, e_pointer);
                        re->mut.unlock();
                    }
                }
            }
            mut_mem_var.lock();
            last = running;
            fresh_add = false;
            mut_mem_var.unlock();
        } while (running);
        mut_main_thread.unlock();
    };
    if (request_manager.main_thread)
    {
        if (request_manager.mut_main_thread.try_lock())
        {
            *request_manager.main_thread = std::thread(f);
            request_manager.mut_main_thread.unlock();
        }
    }
    else
    {
        request_manager.main_thread = new std::thread(f);
    }
}
