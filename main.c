#include <stdio.h>

#include "httpd.h"

int main(void) {
  httpd_init();

  int start_port = 8080;

  while (httpd_start(start_port++) < 0) {
  }

  return 0;
}