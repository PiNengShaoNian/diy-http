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

static const char *mime_find(const char *path) {
  static const http_mime_info_t mime_table[] = {
      {".html", "text/html"},
      {".css", "text/css"},
      {".jpg", "image/jpeg"},
      {".bmp", "image/bmp"},
      {".png", "image/png"},
      {".gif", "image/gif"},
      {".js", "application/x-javascript"},
  };

  for (int i = 0; i < sizeof(mime_table) / sizeof(mime_table[0]); i++) {
    const http_mime_info_t *info = mime_table + i;
    if (strstr(path, info->extension)) {
      return info->content_type;
    }
  }

  return "application/octet-stream";
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

static size_t response_to_text(http_response_t *response, char *buf,
                               size_t size) {
  static const char *property_name_tbl[] = {
      [HTTP_CONTENT_TYPE] = "Content-Type",
      [HTTP_CONTENT_LENGTH] = "Content-Length",
      [HTTP_CONNECTION] = "Connection",
  };
  memset(buf, 0, size);

  sprintf(buf, "%s %s %s\r\n", response->version, response->status,
          response->reason);
  size_t start_size = strlen(buf);
  buf += start_size;

  size_t free_size = size - start_size;
  for (int i = 0; i < HTTPD_PROPERTY_MAX; i++) {
    property_t *curr = response->property + i;

    if (curr->key == HTTP_PROPERTY_NONE) {
      continue;
    }

    const char *name = property_name_tbl[curr->key];
    size_t copy_size = strlen(name) + strlen(curr->value) + 4;
    if (copy_size > free_size) {
      httpd_log("response size error");
      return -1;
    }

    sprintf(buf, "%s: %s\r\n", name, curr->value);
    buf += copy_size;
    start_size += copy_size;
    free_size -= copy_size;
  }

  sprintf(buf, "%s", "\r\n");
  start_size += 2;
  return start_size;
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
  response_add_property(&response, HTTP_CONNECTION, "close");
  response_add_property(&response, HTTP_CONTENT_TYPE, mime_find(path));

  ssize_t size =
      response_to_text(&response, request->data, sizeof(request->data));

  if (send(client->sock, request->data, size, 0) < 0) {
    http_show_error(client, "http send error");
    goto send_error;
  }

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

static int is_cgi_exec(http_request_t *request) {
  static const char *cgi_ext[] = {".py", ".cgi"};

  for (int i = 0; i < sizeof(cgi_ext) / sizeof(cgi_ext[0]); i++) {
    if (strstr(request->url, cgi_ext[i])) {
      return 1;
    }
  }

  return 0;
}

static int cgi_format_param(http_request_t *request, char *param_start) {
  enum {
    CGI_PARAM_NONE,
    CGI_PARAM_NAME,
    CGI_PARAM_VALUE,
  } state = CGI_PARAM_NAME;

  if (param_start == NULL) {
    return 0;
  }

  int count = 0, param_idx = 0;
  char *end = request->url + HTTPD_SIZE_URL;

  while (param_start < end && param_idx < HTTP_CGI_MAX) {
    char c = *param_start;
    if (c == '\0') {
      break;
    } else if (c == '=') {
      *param_start = '\0';
      state = CGI_PARAM_VALUE;
    } else if (c == '&') {
      *param_start = '\0';
      state = CGI_PARAM_NAME;
      param_idx++;
    } else {
      switch (state) {
        case CGI_PARAM_NAME:
          request->cig_param[param_idx].name = param_start;
          state = CGI_PARAM_NONE;
          break;
        case CGI_PARAM_VALUE:
          if (request->cig_param[param_idx].name == NULL) {
            httpd_log("cig param key == 0");
            return -1;
          }

          request->cig_param[param_idx].value = param_start;
          state = CGI_PARAM_NONE;
          count++;
          break;
      }
    }

    param_start++;
  }

  return count;
}

static int method_in(http_client_t *client, http_request_t *request) {
  const char *default_index = "index.html";
  char buf[HTTPD_SIZE_URL];

  char *param_start;
  if ((param_start = strchr(request->url, '?'))) {
    *param_start++ = '\0';
  }

  if (request->url[strlen(request->url) - 1] == '/') {
    snprintf(buf, sizeof(buf), "%s%s%s", root_dir, request->url, default_index);
  } else {
    snprintf(buf, sizeof(buf), "%s%s", root_dir, request->url);
  }

  char *path = buf;
  if (path[0] == '/') {
    path++;
  }

  if (is_cgi_exec(request)) {
    request->param_cnt = cgi_format_param(request, param_start);
    return 0;
  } else {
    return file_normal_send(client, request, path);
  }
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