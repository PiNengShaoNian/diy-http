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
  char *buffer = request->data;
  char *end = request->data + HTTPD_BUF_SIZE;
  ssize_t size;
  while ((size = recv(client->sock, buffer, end - buffer, 0)) > 0) {
    request->data[HTTPD_BUF_SIZE - 1] = '\0';
    httpd_log("http read: %s", buffer);

    buffer += size;
    char *header_end = strstr(request->data, "\r\n\r\n");
    if (header_end) {
      break;
    }
  }

  if ((size < 0) || (request->data == buffer)) {
    http_show_error(client, "recv http header failed.");
    return -1;
  }

  return 0;
}

static int parse_request(http_client_t *client, http_request_t *request) {
  request->data[sizeof(request->data) - 1] = '\0';

  char *curr = request->data;
  request->method = curr;
  if ((curr = strchr(curr, ' ')) == NULL) {
    http_show_error(client, "no method");
    return -1;
  }
  *curr++ = '\0';

  request->url = curr;
  if ((curr = strchr(curr, ' ')) == NULL) {
    http_show_error(client, "no url");
    return -1;
  }
  *curr++ = '\0';

  request->version = curr;
  if ((curr = strstr(curr, "\r\n")) == NULL) {
    http_show_error(client, "no version");
    return -1;
  }
  *curr = '\0';
  curr += 2;

  return 0;
}

static void response_init(http_response_t *response) {
  memset(response, 0, sizeof(http_response_t));
}

static void response_set_start(http_response_t *response, const char *version,
                               const char *status, const char *reason) {
  strcpy(response->version, version);
  strcpy(response->status, status);
  strcpy(response->reason, reason);
}

static void response_add_property(http_response_t *response, property_key_t key,
                                  const char *value) {
  property_t *p = (property_t *)0;

  for (int i = 0; i < HTTPD_PROPERTY_MAX; i++) {
    property_t *curr = response->property + i;
    if (curr->key == HTTP_PROPERTY_NONE && !p) {
      p = curr;
      continue;
    } else if (curr->key == key) {
      p = curr;
      break;
    }
  }

  if (p) {
    p->key = key;
    strncpy(p->value, value, sizeof(p->value) - 1);
  }
}

static int file_normal_send(http_client_t *client, http_request_t *request,
                            const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    http_show_error(client, "get file failed.");
    send_404_not_found(client);
    return -1;
  }

  http_response_t response;
  response_init(&response);
  response_set_start(&response, "HTTP/1.1", "200", "OK");
  // response_add_property(&response, HTTP_CONTENT_LENGTH, )
  response_add_property(&response, HTTP_CONNECTION, "close");

  ssize_t size;
  while ((size = fread(request->data, 1, sizeof(request->data), file)) > 0) {
    if (send(client->sock, request->data, size, 0) < 0) {
      http_show_error(client, "http send error");
      goto send_error;
    }
  }

  fclose(file);
  return 0;
send_error:
  fclose(file);
  return -1;
}

static int method_in(http_client_t *client, http_request_t *request) {
  const char *default_index = "index.html";
  char buf[HTTPD_SIZE_URL];
  if (request->url[strlen(request->url) - 1] == '/') {
    snprintf(buf, sizeof(buf), "%s%s%s", root_dir, request->url, default_index);
  } else {
    snprintf(buf, sizeof(buf), "%s%s", root_dir, request->url);
  }

  char *path = buf;
  if (path[0] == '/') {
    path++;
  }

  return file_normal_send(client, request, path);
}

static int process_request(http_client_t *client, http_request_t *request) {
  if (request->url[0] == '\0' || strstr(request->url, "..")) {
    http_show_error(client, "url is not valid");
    return -1;
  }

  if (strcmp(request->version, "HTTP/1.1") != 0 &&
      strcmp(request->version, "HTTP/1.0") != 0) {
    http_show_error(client, "http version error");
    return -1;
  }

  if (strcmp(request->method, "GET") == 0) {
    request->m_code = HTTP_METHOD_GET;
  } else if (strcmp(request->method, "POST") == 0) {
    request->m_code = HTTP_METHOD_POST;
  } else {
    http_show_error(client, "http method error");
    return -1;
  }

  return method_in(client, request);
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