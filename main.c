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

int cgi_echo(const struct _http_cgi_t* cgi, http_client_t* client,
             http_request_t* request) {
  char* body = request->body;

  const char header[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "\r\n";
  char* msg = strchr(body, '=');
  if (msg == NULL) {
    return -1;
  } else {
    msg++;
    if (*msg == '\0') {
      return -1;
    }
  }

  send(client->sock, header, sizeof(header) - 1, 0);
  return send(client->sock, msg, strlen(msg), 0);
}

static const http_cgi_t cgi_table[] = {
    {
        .url = "/add.cgi",
        .cgi_fun = cgi_add,
    },
    {
        .url = "/echo.cgi",
        .cgi_fun = cgi_echo,
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