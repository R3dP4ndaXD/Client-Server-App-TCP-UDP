#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdio.h>


int recv_all(int sockfd, void *buffer) {
  size_t bytes_received = 0;
  size_t bytes_remaining = sizeof(u_int32_t);
  char *buff = (char *)buffer;
  //se primeste intai prefixul pachetului, el indicand cat de lung este pachetul propriu-zis pe care il preceda
  while (bytes_remaining) {
    ssize_t received = recv(sockfd, buff + 4 - bytes_remaining, bytes_remaining, 0);
    if (received == -1) {
      return -1;
    }
    if (received == 0) {
      return 0;
    }
    bytes_received += received;
    bytes_remaining -= received;
  }

  u_int32_t len;
  memcpy(&len , buff, 4);
  len = ntohl(len);
  bytes_remaining = len;
  //se primeste pachetul
  while (bytes_remaining) { 
    ssize_t received = recv(sockfd, buff + 4 + len - bytes_remaining, bytes_remaining, 0);
    if (received == -1) {
      return -1;
    }
    if (received == 0) {
      return 0;
    }
    bytes_received += received;
    bytes_remaining -= received;
  }
  return bytes_received;
}

int send_all(int sockfd, void *buffer, u_int32_t *len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = 4;
  char *buff = (char *)buffer;
  //se trimite intai lungimea pachetului (in network order), ca prefix al pachetului
  while (bytes_remaining) {
    ssize_t sent =send(sockfd,  len + 4 - bytes_remaining, bytes_remaining, 0);
    if (sent == -1) {
      return -1;
    }
    if (sent == 0) {
      return 0;
    }
    bytes_sent  += sent;
    bytes_remaining -= sent;
  }
  //se trimite pachetul propriu-zis
  bytes_remaining = ntohl(*len);
  while (bytes_remaining) {
    ssize_t sent = send(sockfd,  buff + ntohl(*len) - bytes_remaining, bytes_remaining, 0);
    if (sent == -1) {
      return -1;
    }
    if (sent == 0) {
      return 0;
    }
    bytes_sent  += sent;
    bytes_remaining -= sent;
  }
  return bytes_sent;
}