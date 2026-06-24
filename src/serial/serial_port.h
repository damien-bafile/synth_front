/// @file serial_port.h
/// @brief Cross-platform serial port abstraction (Windows / macOS / Linux).
///
/// This is a plain C interface so it can be linked from both C and C++ code.
/// On Windows the "file descriptor" is actually a HANDLE cast through intptr_t;
/// on Unix it is a real POSIX file descriptor. Callers must not close it with
/// the platform-native close function directly.

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Open a serial device.
/// @param device Path to the serial port (e.g. "/dev/ttyACM0" or "COM3").
/// @param baud   Desired baud rate. Values above 230400 use platform-specific
///               custom-rate ioctls on Linux/macOS.
/// @return       A file descriptor on success, or -1 on failure.
int serial_open(const char* device, int baud);

/// Close a previously opened serial port.
/// @param fd Descriptor returned by serial_open(), or -1 (which is a no-op).
void serial_close(int fd);

/// Read up to @p len bytes from the serial port.
/// @param fd  Open descriptor.
/// @param buf Destination buffer.
/// @param len Maximum bytes to read.
/// @return    Number of bytes read, 0 if no data is currently available (EAGAIN),
///            or -1 on error.
int serial_read(int fd, uint8_t* buf, int len);

/// Write up to @p len bytes to the serial port.
/// @param fd  Open descriptor.
/// @param buf Source buffer.
/// @param len Number of bytes to write.
/// @return    Number of bytes written, or -1 on error.
int serial_write(int fd, const uint8_t* buf, int len);

#ifdef __cplusplus
}
#endif
