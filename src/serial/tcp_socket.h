#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int tcp_connect(const char* host, int port);
void tcp_close(int fd);

#ifdef __cplusplus
}
#endif
