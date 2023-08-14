#ifndef HTTPD_H
#define HTTPD_H

#include <arpa/inet.h>
#include <stdint.h>

#define HTTPD_QUEUE_NR 5
#define HTTPD_BUF_SIZE 1024
#define HTTPD_SIZE_URL 32

typedef struct _http_client_t {
  int sock;
  char ipbuf[INET_ADDRSTRLEN];
  uint16_t port;
} http_client_t;

typedef struct _http_request_t {
  char data[HTTPD_BUF_SIZE];

  enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
  } m_code;

  char* method;
  char* url;
  char* version;
} http_request_t;

void httpd_init(void);
int httpd_start(const char* dir, uint16_t port);

#endif