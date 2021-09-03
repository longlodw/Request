This library is built on top of libcurl to allow non blocking http/https requests.
To use, include the `src/Request.h` file and link with `bin/librequest.so` and `libcurl`
```
char* url = "https://google.com";
char** headers = nullptr;
size_t headers_num = 0;
Request::RequestResult result;
Request::addRequest(result, url, headers, headers_num, Request::RequestType::GET);
Request::startAllRequests();
//other code
char* response = result.getData();
size_t response_length = result.getSize();
```