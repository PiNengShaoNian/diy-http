#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httpd.h"

int cgi_add(const struct _http_cgi_t* cgi, http_client_t* client,
            http_request_t* request) {
  if (request->param_cnt < 2) {
    return -1;
  }

  int num1 = atoi(request->cig_param[0].value);
  int num2 = atoi(request->cig_param[1].value);

  int result = num1 + num2;
  char buf[64];
  int count = snprintf(buf, sizeof(buf),
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/html\r\n"
                       "\r\n"
                       "%d",
                       result);

  return send(client->sock, buf, count, 0);
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