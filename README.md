This library is built on top of libcurl to allow non blocking http/https requests.
To use, include the `src/Request.h` file and link with `bin/librequest.so` and `libcurl`
```
char* url = "https://google.com";
char** headers = nullptr;
size_t headers_num = 0;
char* res = nullptr;
Request::RequestHandler hd;
Request::addRequest(hd, url, reinterpret_cast<Request::void_ptr&>(res), false, headers);
Request::startAllRequests(100);
//other code
size_t response_length = result.join();
```