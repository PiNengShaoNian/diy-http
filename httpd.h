#ifndef HTTPD_H
#define HTTPD_H

#include <arpa/inet.h>
#include <stdint.h>

#define HTTPD_QUEUE_NR 5

void httpd_init(void);
int httpd_start(const char* dir, uint16_t port);

typedef struct _http_client_t {
  int sock;
  char ipbuf[INET_ADDRSTRLEN];
  uint16_t port;
} http_client_t;

#endif