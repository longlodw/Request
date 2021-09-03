#pragma once
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <curl/curl.h>
namespace Request
{
    class RequestResult;
    template <typename E, typename A = std::allocator<E>>
    class Manager
    {
        std::vector<E, A> resources;
        std::vector<size_t> free_positions;
        
        public:
        void reserve(const size_t& n)
        {
            resources.reserve(n);
            free_positions.reserve(n);
        }
        template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, E>>>
        void add(T&& v)
        {
            free_positions.push_back(resources.size());
            resources.push_back(std::forward<T>(v));
        }
        size_t getNextResource(E* result)
        {
            if (free_positions.size())
            {
                size_t p = free_positions.back();
                result = &(resources[p]);
                free_positions.pop_back();
                return p;
            }
            else
            {
                size_t p = resources.size();
                resources.push_back(E());
                result = &(resources[p]);
                return p;
            }
        }
        void returnResource(const size_t& p)
        {
            free_positions.push_back(p);
        }
        template <typename Func>
        bool release(const Func& f)
        {
            if (free_positions.size() != resources.size())
            {
                return false;
            }
            for (auto it = resources.begin(); it < resources.end(); ++it)
            {
                f(*it);
            }
            return true;
        }
        size_t available() const
        {
            return free_positions.size();
        }
    };

    template <size_t N = 0>
    struct RequestManager
    {
        std::map<CURL *, RequestResult *> curl_pointer_map;
        std::thread *main_thread;
        std::mutex mut_main_thread;
        std::mutex mut_mem_var;
        Manager<CURL *> easy_pointers;
        CURLM *multi_pointer;
        int running;
        bool fresh_add = false;

        RequestManager()
        : curl_pointer_map(), main_thread(nullptr), mut_main_thread(), mut_mem_var(), easy_pointers(), multi_pointer(curl_multi_init()), running(0)
        {
            easy_pointers.reserve(N);
            for (size_t k = 0; k < N; ++k)
            {
                easy_pointers.add(curl_easy_init());
            }
        }

        ~RequestManager()
        {
            if(main_thread)
            {
                main_thread->join();
            }
            delete main_thread;
            curl_global_cleanup();
        }
    };
}