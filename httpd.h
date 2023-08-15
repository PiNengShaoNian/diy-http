#ifndef HTTPD_H
#define HTTPD_H

#include <arpa/inet.h>
#include <stdint.h>

#define HTTPD_QUEUE_NR 5
#define HTTPD_BUF_SIZE 1024
#define HTTPD_SIZE_URL 32
#define HTTP_CGI_MAX 10
#define HTTPD_MAX_CLIENT 10

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

  char* body;
  int content_length;

  struct _cgi_param_t {
    const char* name;
    const char* value;
  } cig_param[HTTP_CGI_MAX];

  int param_cnt;
} http_request_t;

typedef struct _http_mime_info_t {
  const char* extension;
  const char* content_type;
} http_mime_info_t;

#define HTTP_SIZE_VERSION 10
#define HTTP_SIZE_STATUS 10
#define HTTP_SIZE_REASON 10
#define HTTPD_PROPERTY_MAX 10
#define HTTP_PROPERTY_VALUE_SIZE 32

typedef enum _property_key_t {
  HTTP_PROPERTY_NONE = 0,
  HTTP_CONTENT_TYPE,
  HTTP_CONTENT_LENGTH,
  HTTP_CONNECTION,
} property_key_t;

typedef struct _property_t {
  property_key_t key;
  char value[HTTP_PROPERTY_VALUE_SIZE];
} property_t;

typedef struct _http_response_t {
  char version[HTTP_SIZE_VERSION];
  char status[HTTP_SIZE_STATUS];
  char reason[HTTP_SIZE_REASON];

  property_t property[HTTPD_PROPERTY_MAX];
} http_response_t;

typedef struct _http_cgi_t {
  const char* url;
  int (*cgi_fun)(const struct _http_cgi_t* cgi, http_client_t* client,
                 http_request_t* request);
} http_cgi_t;

void httpd_init(const http_cgi_t* table);
int httpd_start(const char* dir, uint16_t port);

#endif