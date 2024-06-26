#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include "./common.h"
#include "helpers.h"

using namespace std;
#define MAX_CONNECTIONS 32
const char *exit_str = "exit";
const char *subscribe_str = "subscribe ";
const char *unsubscribe_str = "unsubscribe ";

// structura ce retine datele unui client tcp
typedef struct client_info {
  char id[11];
  vector<string> subscriptions;
} client_info;

vector<client_info> clients;                //vector al tutoror clientilor
vector<pair<int, int>> connected_clients;   //vector al clientilor conectati la server identificati prin
                                            //indexul in vectorul de clinetii si file descriptor-ul conextiunii  

//sparge un topic pe nivele
vector<string> splitString(const string& input, char delimiter) {
    vector<string> tokens;
    stringstream ss(input);
    string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
//verifica daca un topic e acoperit de o subscriptie cu wildcard 
bool match(vector<string>::const_iterator subIter,
           vector<string>::const_iterator subEnd,
           vector<string>::const_iterator topIter,
           vector<string>::const_iterator topEnd) {
    
  if (subIter == subEnd && topIter == topEnd)
    return true;

  if (*subIter == "*" && subIter + 1 != subEnd && topIter == topEnd) 
    return false; 

  if (*subIter == "*" && subIter + 1 == subEnd)
    return true;

  if (*subIter == "+" && subIter + 1 == subEnd && topIter + 1 < topEnd)
    return false;

  if (*subIter == "+" && topIter + 1 == subEnd)
    return true;
  
  if (*subIter == "+" || *subIter == *topIter) 
    return match(subIter + 1, subEnd, topIter + 1, topEnd); 
      
  if (*subIter == "*") 
    return match(subIter + 1,subEnd, topIter, topEnd) || match(subIter,subEnd,  topIter + 1, topEnd);

  return false; 
}

void run_server(int listenfd_tcp, int fd_udp) {

  struct pollfd poll_fds[MAX_CONNECTIONS];
  int nr_clients = 3;
  int rc;
  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0,MSG_MAXSIZE + 1);
  char *msg;
  u_int32_t len;
  
  // Setam socketi listenfd_tcp pentru ascultare
  rc = listen(listenfd_tcp, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // se adauga fd pe care se asteapta initiaza conexiuni(cei doi socketi) si fd pentru stdin
  // multimea read_fds
  poll_fds[0].fd = listenfd_tcp;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = fd_udp;
  poll_fds[1].events = POLLIN;
  poll_fds[2].fd = STDIN_FILENO;
  poll_fds[2].events = POLLIN;

  vector <int> ignored_fds; //fd activati la primirea pachetului cu id-ul 
                            //inediat dupa acceptartea unui conexiuni cu un client tcp
                            //activarile trebuie ignorate deoarece receive-ul a fost deja facut,
                            //iar urmatoarele activarii vor fi captate in poll-uile urmatoare  
  while (1) {
    rc = poll(poll_fds, nr_clients, -1);
    DIE(rc < 0, "poll");
    ignored_fds.clear();
    for (int i = 0; i < nr_clients; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == listenfd_tcp) {
          // a venit o cerere de conexiune pe socketul tcp inactiv (cel cu listen),
          // pe care serverul o accepta
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          int newsockfd = accept(listenfd_tcp, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");
          int rc = recv_all(newsockfd, buf);
          DIE(rc < 0, "recv_all");
          if (rc == 0) {
            // conexiunea s-a inchis
            close(newsockfd);
            continue;
          }
          bool gasit = false;
          char *msg = buf + sizeof(len);
          //verific daca clientul e deja conectat
          for(auto [i, fd]: connected_clients) {
            if (strncmp(clients[i].id, msg, sizeof(clients[i].id)) == 0) {
              gasit = true;
              break;
            }
          }
          if (gasit) {
            //inchid noua conexiunea si clientul
            printf("Client %s already connected.\n", msg);
            memset(buf, 0, sizeof(buf));
            memcpy(buf, exit_str, strlen(exit_str));
            len = htonl(strlen(exit_str) + 1);
            rc = send_all(newsockfd, buf, &len);
            DIE(rc < 0, "send_all");
            close(newsockfd);
            continue;
          }
          // se adauga noul socket intors de accept() la multimea descriptorilor
          // de citire
          poll_fds[nr_clients].fd = newsockfd;
          poll_fds[nr_clients].events = POLLIN;
          nr_clients++;
          ignored_fds.push_back(newsockfd);
          gasit = false;
          for(int i = 0; i < clients.size(); i++) {
            //reconnect
            if (strncmp(clients[i].id, msg, sizeof(clients[i].id)) == 0) {
              gasit = true;
              connected_clients.push_back({i, newsockfd});
              break;
            }
          }
          if (!gasit) {
            //client conectat pentru prima oara
            client_info new_client;
            memset(&new_client, 0, sizeof(client_info));
            memcpy(new_client.id, msg, sizeof(clients[i].id));
            connected_clients.push_back({clients.size(), newsockfd});
            clients.push_back(new_client);
          }
          printf("New client %s connected from %s:%d.\n", msg, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port) );
        } else if (poll_fds[i].fd == fd_udp) {
          // a venit un mesaj de la un client udp
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          rc = recvfrom(fd_udp, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(rc < 0, "recvform");
          if (rc == 0) {
            continue;
          }
          
          char aux[51];
          memset(aux, 0, sizeof(aux));
          memcpy(aux, buf, 50);
          char delimiter = '/';
          string topic(aux);
          //caut clientii conectati care au cel putin un abonament care include topicul mesajului
          for (auto [i, fd]: connected_clients) {
            bool gasit = false;
            for (string subscription: clients[i].subscriptions) {
              vector<string> sub = splitString(subscription, delimiter);
              vector<string> top = splitString(topic, delimiter); 
              if (match(sub.begin(), sub.end(), top.begin(), top.end())) {
                gasit = true;
                break;
              }
            }
            if (gasit) {
              //trimit mesajul la clientul tcp
              len = htonl(rc + 1);
              for (int j = 0; j < nr_clients; j++) {
                if (poll_fds[j].fd == fd) {
                  send_all(poll_fds[j].fd, buf, &len);
                  break;
                }
              }
            }
          }
        } else if (poll_fds[i].fd == STDIN_FILENO) {
          fgets(buf, sizeof(buf), stdin);
          buf[strlen(buf) - 1] = 0;
          if (strncmp(buf, exit_str, strlen(buf)) == 0) {
            //inchid toate conexiunile si anunt toti clientii sa se inchida 
            len = htonl(strlen(buf) + 1);
            for(int j = 3; j < nr_clients; j++) {
              rc = send_all(poll_fds[j].fd, buf, &len);
              DIE(rc < 0, "send_all");
              close(poll_fds[j].fd);
            }
            return;             
          }
        } else {
          if (find(ignored_fds.begin(), ignored_fds.end(), poll_fds[i].fd) != ignored_fds.end()) {
            continue;
          }
          // s-au primit date pe unul din socketii de client,
          // asa ca serverul trebuie sa le receptioneze
          int rc = recv_all(poll_fds[i].fd, buf);
          DIE(rc < 0, "recv_all");
          if (rc == 0) {
            // conexiunea s-a inchis brusc
            for(auto [j, fd]: connected_clients) {
              if (fd == poll_fds[i].fd) {
                fprintf(stderr, "Clientul %s a inchis conexiunea\n", clients[j].id);
                break;
              }
            }
            close(poll_fds[i].fd);
            // se scoate din multimea de citire socketul inchis
            for (int j = i; j < nr_clients - 1; j++) {
              poll_fds[j] = poll_fds[j + 1];
            }
            nr_clients--;
          } else {
            char *id = buf + sizeof(len); 
            msg = id + 11;
            u_int32_t msg_len = rc - sizeof(len) - 11;
            if (strncmp(msg, exit_str, msg_len) == 0) {
              printf("Client %s disconnected.\n", id);
              for (auto it = connected_clients.begin(); it != connected_clients.end(); it++) {
                if (strcmp(clients[(*it).first].id, id) == 0) {
                  connected_clients.erase(it); 
                  break;
                }
              }
              close(poll_fds[i].fd);
              for (int j = i; j < nr_clients - 1; j++) {
                poll_fds[j] = poll_fds[j + 1];
              }
              nr_clients--;
            } else if (strncmp(msg, subscribe_str, strlen(subscribe_str)) == 0) {
              char *topic = msg + strlen(subscribe_str);
              for (auto [j, fd]: connected_clients) {
                if (strcmp(clients[j].id, id) == 0) {
                  string sub(topic);
                  clients[j].subscriptions.push_back(sub);
                  break;
                }
              }
            } else if (strncmp(msg, unsubscribe_str, strlen(unsubscribe_str)) == 0) {
              char *topic = msg + strlen(unsubscribe_str);
              for (auto [i, fd]: connected_clients) {
                if (strcmp(clients[i].id, id) == 0) {
                  string sub(topic);
                  auto it = find(clients[i].subscriptions.begin(), clients[i].subscriptions.end(), sub); 
                  if (it != clients[i].subscriptions.end()) { 
                    clients[i].subscriptions.erase(it); 
                  }
                  break;
                }
              } 
            } else {
              fprintf(stderr, "S-a primit mesajul \"%s\" de la clientul %s\n", msg, id);
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  if (argc != 2) {
    fprintf(stderr, "\n Usage: %s <port>\n", argv[0]);
    return 1;
  }

  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru receptionarea conexiunilor de la clientii TCP
  int listenfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
  DIE(listenfd_tcp < 0, "socket TCP");

  // Obtinem un socket UDP pentru receptionarea conexiunilor de la clientii UDP
  int fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(fd_udp < 0, "socket UDP");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // Facem adresele socket-ilor reutilizabile, ca sa nu primim eroare in caz ca
  // rulam de 2 ori rapid
  int enable = 1;
  if (setsockopt(listenfd_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed"); 
  if (setsockopt(fd_udp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  //dezactivez algoritmul Nagle
  if (setsockopt(listenfd_tcp, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Asociem adresa serverului cu socketii creati folosind bind
  rc = bind(listenfd_tcp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind tcp");

  rc = bind(fd_udp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind udp");
  run_server(listenfd_tcp, fd_udp);

  // Inchidem listenfd_tcp si fd_udp
  close(listenfd_tcp);
  close(fd_udp);
  return 0;
}