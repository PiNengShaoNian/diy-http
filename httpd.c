#include "httpd.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char *root_dir;

#define httpd_log(fmt, ...)     \
  {                             \
    printf("httpd:");           \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n");               \
  }

void http_show_error(http_client_t *client, const char *msg) {
  printf("client: %s port %d, error: %s\n", client->ipbuf, client->port, msg);
}

static int send_404_not_found(http_client_t *client) {
  static const char response[] =
      "HTTP/1.1 404 File Not Found\r\n"
      "Content-Type: text/html\r\n"
      "\r\n"
      "<html><head><meta "
      "charset=\"UTF-8\"><title>文件未找到</title></head><body><h1>找不到</"
      "h1></"
      "body></html>";
  return send(client->sock, response, sizeof(response), 0);
}

static int read_request(http_client_t *client, http_request_t *request) {
  return 0;
}

static int parse_request(http_client_t *client, http_request_t *request) {
  return 0;
}

static int process_request(http_client_t *client, http_request_t *request) {
  return 0;
}

static void client_handler(http_client_t *client) {
  http_request_t request;

  memset(&request, 0, sizeof(request));

  if (read_request(client, &request) < 0) {
    http_show_error(client, "read request error");
    goto client_end;
  }

  if (parse_request(client, &request) < 0) {
    http_show_error(client, "parse request error");
    goto client_end;
  }

  if (process_request(client, &request) < 0) {
    http_show_error(client, "process request error");
    goto client_end;
  }

client_end:
  httpd_log("close request");
}

void httpd_init(void) {}

int httpd_start(const char *dir, uint16_t port) {
  root_dir = dir ? dir : "";

  int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (server_socket < 0) {
    httpd_log("create socket failed.");
    goto start_error;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_socket, (const struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    httpd_log("bind error");
    goto start_error;
  }

  if (listen(server_socket, HTTPD_QUEUE_NR) < 0) {
    httpd_log("listen error");
    goto start_error;
  }

  httpd_log("server is running, port: http://127.0.0.1:%d", port);
  for (;;) {
    http_client_t *client = (http_client_t *)malloc(sizeof(http_client_t));
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    client->sock = accept(server_socket, (struct sockaddr *)&addr, &len);
    if (client->sock < 0) {
      httpd_log("accept error");
      free(client);
      goto start_error;
    }

    client->port = ntohs(addr.sin_port);
    inet_ntop(AF_INET, &addr.sin_addr, client->ipbuf, INET_ADDRSTRLEN);
    httpd_log("new client %s, port: %d", client->ipbuf, client->port);
    client_handler(client);

    httpd_log("close request");
    close(client->sock);
    free(client);
  }

  close(server_socket);
  return 0;

start_error:
  if (server_socket > 0) {
    close(server_socket);
  }
  return -1;
}