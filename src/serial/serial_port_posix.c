#include "serial_port.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <stdio.h>

#ifdef __APPLE__
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#endif

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

#ifdef __APPLE__
  if (baud > 230400) {
    if (ioctl(fd, IOSSIOSPEED, &baud) < 0) {
      perror("IOSSIOSPEED");
    }
  }
#endif

  return fd;
}

void serial_close(int fd) {
  if (fd >= 0) close(fd);
}

int serial_read(int fd, uint8_t* buf, int len) {
  int n = read(fd, buf, len);
  if (n < 0 && errno == EAGAIN) return 0;
  return n;
}

int serial_write(int fd, const uint8_t* buf, int len) {
  return write(fd, buf, len);
}
