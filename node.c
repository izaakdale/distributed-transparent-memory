#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <string.h>

#include "common.h"

unsigned int n_of_nodes = 3;

node_t new_node()
{
  node_t n;
  printf("IP addr : ");
  char *ip_addr = "127.0.0.1";
  // scanf("%s", &ip_addr);
  n.ip_addr = inet_addr(ip_addr);
  // for now the ip is the same
  n.next_hop_ip = n.ip_addr;

  printf("Node number (1,2,3) / 3 : ");
  scanf("%u", &n.id);
  if (n.id < 1 || n.id > 3)
  {
    printf("please select 1, 2, or 3\n");
    exit(EXIT_FAILURE);
  }

  n.tcp_port = (n.id * n.id) + 2000;
  n.udp_port = (n.id * n.id) + 2001;

  int rem = n.id % 3;
  n.next_hop_udp_port = ((rem + 1) * (rem + 1)) + 2001;

  char buf[15];
  inet_ntop(AF_INET, &n.ip_addr, buf, 15);
  printf("Server %u - %s:%u(tcp)/%u(udp)\n", n.id, buf, n.tcp_port, n.udp_port);
  printf("Next hop udp port: %d\n", n.next_hop_udp_port);
  return n;
}

void udp_socket(node_t *n)
{
  unsigned int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_sock < 0)
  {
    printf("error initiating master socket");
    exit(EXIT_FAILURE);
  }

  sockaddr_in srv;
  srv.sin_family = AF_INET;
  srv.sin_addr.s_addr = INADDR_ANY;
  srv.sin_port = htons(n->udp_port);

  printf("UDP port assigned is %d\n", ntohs(srv.sin_port));

  if ((bind(udp_sock, (const sockaddr *)&srv, sizeof(srv))) < 0)
  {
    printf("bind failure\n");
    exit(EXIT_FAILURE);
  }

  n->udp_socket_fd = udp_sock;
}

void forwarding_udp_socket(node_t *n)
{
  int fus = socket(AF_INET, SOCK_DGRAM, 0);
  if (fus < 0)
  {
    printf("error initiating master socket");
    exit(EXIT_FAILURE);
  }

  sockaddr_in remote;
  remote.sin_addr.s_addr = n->ip_addr;
  remote.sin_family = AF_INET;
  remote.sin_port = htons(n->next_hop_udp_port);

  n->next_hop_udp_socket_fd = fus;
  n->next_hop_addr = remote;
}

void processMessage(node_t n)
{
  message_t msg;
  sockaddr_in cli;
  unsigned int client_address_size = sizeof(cli);
  if (recvfrom(n.udp_socket_fd, &msg, sizeof(message_t), 0, (struct sockaddr *)&cli, &client_address_size) < 0)
  {
    printf("recvfrom()");
    exit(4);
  }

  printf("Received message type %d with key %u\n", msg.type, msg.key);

  if ((msg.key % 3) + 1 == n.id)
  {
    printf("Message reached destination\n");
    switch (msg.type)
    {
    case MESSAGE_TYPE_GET_FORWARD:
      printf("GET VALUE\n");
      break;
    case MESSAGE_TYPE_PUT_FORWARD:
      printf("SET VALUE\n");
      break;
    default:
      break;
    }
  }
  else
  {
    printf("FORWARD IT\n");

    if (sendto(n.next_hop_udp_socket_fd, &msg, sizeof(message_t), 0, (struct sockaddr *)&n.next_hop_addr, sizeof(n.next_hop_addr)) < 0)
    {
      printf("sendto()");
      exit(2);
    }
  }
}

void processInput(node_t n, char *buf)
{
  char *method = strtok(buf, " ");

  if (strncmp(method, "GET", 3) == 0)
  {
    char *keystr = strtok(NULL, "\n");
    printf("hitting get %s\n", keystr);

    int key = atoi(keystr);
    if (key == 0)
    {
      printf("Value cannot be 0 or a string\n");
      return;
    }
    if ((key % 3) + 1 == n.id)
    {
      printf("GET VALUE AND PRINT IT\n");
    }
    else
    {
      printf("CREATE MESSAGE AND FORWARD IT\n");

      message_t *msg = newMessage(MESSAGE_TYPE_GET_FORWARD, key, 0, n.ip_addr, n.tcp_port);

      if (sendto(n.next_hop_udp_socket_fd, msg, sizeof(message_t), 0, (struct sockaddr *)&n.next_hop_addr, sizeof(n.next_hop_addr)) < 0)
      {
        printf("sendto()");
        exit(2);
      }
    }
  }
  else if (strncmp(method, "SET", 3) == 0)
  {
    char *keystr = strtok(NULL, " ");
    printf("hitting set %s\n", keystr);
    char *valuestr = strtok(NULL, "\n");
    printf("hitting set %s\n", valuestr);
    int key = atoi(keystr);
    int value = atoi(valuestr);
    if (key == 0 || value == 0)
    {
      printf("Value cannot be 0 or a string\n");
      return;
    }

    if ((key % 3) + 1 == n.id)
    {
      printf("SET VALUE IN MEMORY\n");
    }
    else
    {
      printf("CREATE MESSAGE AND FORWARD IT\n");

      message_t *msg = newMessage(MESSAGE_TYPE_PUT_FORWARD, key, value, n.ip_addr, n.tcp_port);
      if (sendto(n.next_hop_udp_socket_fd, msg, sizeof(message_t), 0, (struct sockaddr *)&n.next_hop_addr, sizeof(n.next_hop_addr)) < 0)
      {
        printf("sendto()");
        exit(2);
      }
    }
  }
  else
  {
    printf("ERR - not an option\n");
  }
}

int main(int argc, char const *argv[])
{
  node_t n = new_node();
  udp_socket(&n);
  forwarding_udp_socket(&n);

  fd_set fdr;
  while (1)
  {
    printf("Waiting for user input in the form of GET key (int) / SET key (int) value (int)\n");
    FD_ZERO(&fdr);
    FD_SET(0, &fdr); // add STDIN to the fd set
    FD_SET(n.udp_socket_fd, &fdr);

    select(n.udp_socket_fd + 1, &fdr, NULL, NULL, NULL);

    if (FD_ISSET(0, &fdr))
    {
      char buf[BUFSIZ];
      size_t count = read(0, buf, BUFSIZ);
      processInput(n, buf);
    }

    if (FD_ISSET(n.udp_socket_fd, &fdr))
    {
      processMessage(n);
    }
  }

  return 0;
}
