#pragma once

#include <string>
#include <unordered_map>

namespace download {

  /*
    功能：
      HTTP 响应解析器。构造时传入原始响应文本并立即完成解析，
      内部响应结构不对外暴露，只提供查询接口
  */
  class HttpParser {
  public:

    HttpParser(const HttpParser&) = delete;
    HttpParser& operator=(const HttpParser&) = delete;

    /*
      参数：
        raw_response: 原始 HTTP 响应文本
    */
    HttpParser(const std::string& raw_response);

    /*
      功能：
        读取 header 值
      参数：
        key: header 名
      返回值：
        对应的值；不存在时返回空字符串
    */
    std::string get(const std::string& key) const;

    int status_code() const;

    std::string reason() const;

    std::string version() const;

  private:

    /*
      功能：
        解析 HTTP 响应文本（构造时调用）
    */
    void parse(const std::string& s);

    struct HttpResponse {
      std::string version;
      int status_code = 0;
      std::string reason;
      std::unordered_map<std::string, std::string> headers;
    };

    HttpResponse response_;
  };

} // namespace download
