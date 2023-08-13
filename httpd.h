#ifndef HTTPD_H
#define HTTPD_H

#include <stdint.h>

#define HTTPD_QUEUE_NR 5

void httpd_init(void);
int httpd_start(uint16_t port);

#endif