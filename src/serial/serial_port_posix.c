#include "serial_port.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <stdio.h>

#ifdef __APPLE__
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <asm/termbits.h>
#endif

// Convert a numeric baud rate to a termios speed_t constant.
static speed_t baud_to_speed(int baud) {
  switch (baud) {
    case 0:       return B0;
    case 50:      return B50;
    case 75:      return B75;
    case 110:     return B110;
    case 134:     return B134;
    case 150:     return B150;
    case 200:     return B200;
    case 300:     return B300;
    case 600:     return B600;
    case 1200:    return B1200;
    case 1800:    return B1800;
    case 2400:    return B2400;
    case 4800:    return B4800;
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    default:      return B230400;
  }
}

// Open and configure a serial device (8N1, raw mode) at the given baud rate.
int serial_open(const char* device, int baud) {
  int fd = open(device, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror("serial_open");
    return -1;
  }

  struct termios tty;
  if (tcgetattr(fd, &tty) < 0) {
    perror("tcgetattr");
    close(fd);
    return -1;
  }

  cfmakeraw(&tty);

  speed_t speed = baud_to_speed(baud);
  cfsetispeed(&tty, speed);
  cfsetospeed(&tty, speed);

  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);

  tty.c_cc[VMIN]  = 0;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) < 0) {
    perror("tcsetattr");
    close(fd);
    return -1;
  }

  if (baud > 230400) {
#ifdef __APPLE__
    if (ioctl(fd, IOSSIOSPEED, &baud) < 0) {
      perror("IOSSIOSPEED");
    }
#elif defined(__linux__)
    struct termios2 t2;
    if (ioctl(fd, TCGETS2, &t2) < 0) {
      perror("TCGETS2");
    } else {
      t2.c_cflag &= ~CBAUD;
      t2.c_cflag |= BOTHER;
      t2.c_ispeed = baud;
      t2.c_ospeed = baud;
      if (ioctl(fd, TCSETS2, &t2) < 0) {
        perror("TCSETS2");
      }
    }
#endif
  }

  return fd;
}

// Close the serial port file descriptor.
void serial_close(int fd) {
  if (fd >= 0) close(fd);
}

// Read from serial port; returns 0 on EAGAIN instead of -1.
int serial_read(int fd, uint8_t* buf, int len) {
  int n = read(fd, buf, len);
  if (n < 0 && errno == EAGAIN) return 0;
  return n;
}

// Write bytes to the serial port; returns result of write().
int serial_write(int fd, const uint8_t* buf, int len) {
  return write(fd, buf, len);
}
