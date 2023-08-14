#ifndef HTTPD_H
#define HTTPD_H

#include <arpa/inet.h>
#include <stdint.h>

#define HTTPD_QUEUE_NR 5
#define HTTPD_BUF_SIZE 1024

typedef struct _http_client_t {
  int sock;
  char ipbuf[INET_ADDRSTRLEN];
  uint16_t port;
} http_client_t;

typedef struct _http_request_t {
  char data[HTTPD_BUF_SIZE];
} http_request_t;

void httpd_init(void);
int httpd_start(const char* dir, uint16_t port);

#endif