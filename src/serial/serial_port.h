#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int serial_open(const char* device, int baud);
void serial_close(int fd);
int serial_read(int fd, uint8_t* buf, int len);
int serial_write(int fd, const uint8_t* buf, int len);

#ifdef __cplusplus
}
#endif
