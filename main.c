#include <stdio.h>

#include "httpd.h"

int cgi_add(const struct _http_cgi_t* cgi, http_client_t* client,
            http_request_t* request) {
  return 0;
}

static const http_cgi_t cgi_table[] = {
    {
        .url = "/add.cgi",
        .cgi_fun = cgi_add,
    },
    {.url = 0, .cgi_fun = 0},
};

int main(void) {
  httpd_init(cgi_table);

  int start_port = 8080;

  while (httpd_start("./htdocs", start_port++) < 0) {
  }

  return 0;
}