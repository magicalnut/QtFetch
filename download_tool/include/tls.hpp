#pragma once

#include <cstddef>
#include <string>
#include <openssl/ossl_typ.h>

namespace download {

class SSL_CTX_manager {
public:
  SSL_CTX_manager();
  ~SSL_CTX_manager(); 
  SSL_CTX* get_ctx() noexcept;
  SSL_CTX_manager(const SSL_CTX_manager&) = delete;
  SSL_CTX_manager& operator=(const SSL_CTX_manager&) = delete;

private:
  SSL_CTX* create_ctx();

  SSL_CTX* ctx;
};



class connection {
public:
  virtual ~connection() = 0;
  virtual int  send(const char* buf, size_t n) = 0;
  virtual int  recv(char* buf, size_t n)       = 0;
  virtual void close_connection()              = 0;

  static int find_server_socket(std::string hostname, std::string port = "80");

  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

protected:
  connection() = default;
};

class tls_connection : public connection {
public:

  tls_connection(const std::string& host, const std::string& port,SSL_CTX* ctx);
  ~tls_connection() override;

  int  send(const char* buf, size_t n) override;
  int  recv(char* buf, size_t n)       override;
  void close_connection()              override;

private:
  SSL* ssl=nullptr;
  int  socket_fd=-1;
  std::string host_;
};



class tcp_connection : public connection {
public:
  explicit tcp_connection(const std::string& host, const std::string& port);
  ~tcp_connection() override;

  int  send(const char* buf, size_t n) override;
  /*
    补充带flag标志位的重载版本
  */
  int  send(const char* buf, size_t n, int flag);
  /*
    为形式统一的重载版本
  */
  int  send(int _, const char* buf, size_t n, int flag);

  int  recv(char* buf, size_t n)       override;
  void close_connection()              override;

private:
  int socket_fd=-1;
};


} // namespace download
