#include "http_parser.hpp"

namespace download {

  HttpParser::HttpParser(const std::string& raw_response){
    parse(raw_response);
  }

  void HttpParser::parse(const std::string& s){
    auto cut = [](const std::string& s) -> std::string {
      int start = s.find_first_not_of(" \t\r\n");
      int end = s.size();
      while(end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\r' || s[end-1] == '\n')){
        end--;
      }
      return s.substr(start, end-start);
    };

    int p1 = s.find(' ');
    int p2 = s.find(' ', p1+1);
    int ed = s.find("\r\n");//新增
    response_.version = cut(s.substr(0,p1));//截取协议版本
    response_.status_code = std::stoi(cut(s.substr(p1+1, p2-p1-1)));
    response_.reason = cut(s.substr(p2+1,ed-(p2+1)));//截取存储状态码对应的文本描述

    while(true){ 
      ed += 2;
      int p = s.find(':', ed);
      if(p == std::string::npos)  break;

      std::string fi = cut(s.substr(ed, p-ed));
      ed = s.find("\r\n", ed);
      std::string se = cut(s.substr(p+1, ed-p));
      
      response_.headers[fi] = se;
    }
  }

  std::string HttpParser::get(const std::string& key) const {
    auto it = response_.headers.find(key);
    return it == response_.headers.end() ? "" : it->second;
  }

  int HttpParser::status_code() const {
    return response_.status_code;
  }

  std::string HttpParser::reason() const {
    return response_.reason;
  }

  std::string HttpParser::version() const {
    return response_.version;
  }

} // namespace download
