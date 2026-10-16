#pragma once

#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <unordered_map>

namespace download {

  struct HttpResponse {
    int status_code;
    std::unordered_map<std::string, std::string> headers; // key 小写
    std::string body;
  };
  

} // namespace download
