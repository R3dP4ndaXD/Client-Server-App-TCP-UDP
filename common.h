#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int recv_all(int sockfd, void *buffer);
int send_all(int sockfd, void *buffer, u_int32_t *len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1551


#endif
