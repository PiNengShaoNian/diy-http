#include "httpd.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void httpd_init(void) {}

int httpd_start(uint16_t port) {
  int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (server_socket < 0) {
    goto start_error;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_socket, (const struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    goto start_error;
  }

  if (listen(server_socket, HTTPD_QUEUE_NR) < 0) {
    goto start_error;
  }

  close(server_socket);
  return 0;

start_error:
  if (server_socket > 0) {
    close(server_socket);
  }
  return -1;
}