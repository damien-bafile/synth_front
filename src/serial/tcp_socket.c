#include "tcp_socket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

// Create a TCP socket and connect to host:port; returns fd or -1 on error.
int tcp_connect(const char* host, int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
    perror("inet_pton");
    close(fd);
    return -1;
  }

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("connect");
    close(fd);
    return -1;
  }

  return fd;
}

// Close the TCP connection.
void tcp_close(int fd) {
  if (fd >= 0) close(fd);
}
