#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <netinet/tcp.h>
#include "./common.h"
#include "helpers.h"

const char *exit_msg = "exit";
const char *subscribe_str = "subscribe ";
const char *unsubscribe_str = "unsubscribe ";
char id[11];

void run_client(int sockfd) {
  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);
  char *msg;
  u_int32_t len;
  struct pollfd poll_fds[2];
  int rc;
  // se adauga noul file descrioorul pentru accept si stdin in
  // multimea read_fds
  poll_fds[0].fd = STDIN_FILENO;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = sockfd;
  poll_fds[1].events = POLLIN;
  
  while (1) {
    rc = poll(poll_fds, 2, 0);
    DIE(rc < 0, "poll");
    for (int i = 0; i < 2; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == STDIN_FILENO) {
          memset(buf, 0, sizeof(buf));
          memcpy(buf, id, sizeof(id));
          msg = buf + sizeof(id);
          fgets(msg, sizeof(buf) - sizeof(id), stdin);
          msg[strlen(msg)- 1] = 0;
          len = htonl(sizeof(id) + strlen(msg) + 1);
          rc = send_all(sockfd, buf, &len);
          DIE(rc <= 0, "send id");
          
          if (strncmp(msg, exit_msg, strlen(msg)) == 0) {
            close(sockfd);
            return;
          } else if (strncmp(msg, subscribe_str, strlen(subscribe_str)) == 0) {
            printf("Subscribed to topic %s\n", msg + strlen(subscribe_str));
          } else if (strncmp(msg, unsubscribe_str, strlen(unsubscribe_str)) == 0) {
            printf("Unsubscribed from topic %s\n", msg + strlen(unsubscribe_str));
          }
        } else {
          // Receive a message and show it's content
          rc = recv_all(sockfd, buf);
          msg = buf + sizeof(len);
          DIE(rc < 0, "recv_all");
          if (rc == 0) {
            close(sockfd);
            return;
          }
          if (strncmp(msg, exit_msg, strlen(msg)) == 0) {
            close(sockfd);
            return;
          }
          //parsare mesaj udp
          char topic[51];
          strncpy(topic, msg, 50);
          unsigned char type = msg[50];
          switch (type) {
          case 0:
            char sign;
            sign = msg[51];
            u_int32_t number1;
            memcpy(&number1, &msg[52], sizeof(number1));
            if (sign == 0 || number1 == 0) {
              printf("%s - INT - %u\n", topic, ntohl(number1));
            } else {
              printf("%s - INT - -%u\n", topic, ntohl(number1));
            }
            break;
          case 1:
            u_int16_t number2;
            memcpy(&number2, &msg[51], sizeof(number2));
            printf("%s - SHORT_REAL - %.2f\n", topic, ntohs(number2) / 100.0);
            break;
          case 2:
            sign = msg[51];
            u_int32_t number3;
            u_int8_t exp;
            memcpy(&number3, &msg[52], sizeof(number3));
            exp = msg[56];
            if (sign == 0) {
              printf("%s - FLOAT - %.4f\n", topic, ntohl(number3) * pow(10, -exp));
            } else {
              printf("%s - FLOAT - -%.4f\n", topic, ntohl(number3) * pow(10, -exp));
            }
            break;
          case 3:
            printf("%s - STRING - %s\n", topic, &msg[51]);
            break;
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  int sockfd = -1;
  
  if (argc != 4) {
    fprintf(stderr, "\n Usage: %s <id> <ip> <port>\n", argv[0]);
    return 1;
  }
  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru conectarea la server
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  // Facem adresele socketului reutilizabila, ca sa nu primim eroare in caz ca
  // rulam de 2 ori rapid
  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed"); 
  //dezactivez algoritmul Nagle
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Ne conectăm la server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  strncpy(id, argv[1], sizeof(id) - 1);
  //tirmit primul pachet care contine id-ul catre server
  u_int32_t len = htonl(sizeof(id));
  rc = send_all(sockfd, id, &len);
  DIE(rc <= 0, "send id");

  run_client(sockfd);

  // Inchidem conexiunea si socketul creat
  close(sockfd);

  return 0;
}