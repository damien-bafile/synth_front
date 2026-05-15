#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Connect to a TCP host:port; returns fd or -1 on failure.
int tcp_connect(const char* host, int port);
// Close a TCP connection.
void tcp_close(int fd);

#ifdef __cplusplus
}
#endif
