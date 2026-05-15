#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Open a serial device with the given baud rate; returns fd or -1 on failure.
int serial_open(const char* device, int baud);
// Close a previously opened serial port.
void serial_close(int fd);
// Read bytes from the serial port; returns number read, 0 on EAGAIN, or -1 on error.
int serial_read(int fd, uint8_t* buf, int len);
// Write bytes to the serial port; returns number written or -1 on error.
int serial_write(int fd, const uint8_t* buf, int len);

#ifdef __cplusplus
}
#endif
