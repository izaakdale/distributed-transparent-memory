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

typedef struct record
{
  int key;
  int value;
} record_t;

record_t store[1024];

void init_store()
{
  for (int i = 0; i < 1024; i++)
  {
    store[i].key = -1;
    store[i].value = -1;
  }
}

record_t fetch(int key)
{
  for (int i = 0; i < 1024; i++)
  {
    if (store[i].key == key)
    {
      return store[i];
    }
  }
  record_t undef_rec;
  undef_rec.key = -1;
  undef_rec.value = -1;
  return undef_rec;
}

void insert(int key, int value)
{
  for (int i = 0; i < 1024; i++)
  {
    if (store[i].key == -1)
    {
      record_t new_rec;
      new_rec.key = key;
      new_rec.value = value;
      store[i] = new_rec;
    }
  }
}

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
  printf("Next hop UDP port: %d\n", n.next_hop_udp_port);
  return n;
}

void udp_socket(node_t *n)
{
  unsigned int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_sock < 0)
  {
    printf("error initiating master udp socket");
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

void tcp_socket(node_t *n)
{
  unsigned int tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (tcp_sock < 0)
  {
    printf("error initiating master tcp socket");
    exit(EXIT_FAILURE);
  }

  sockaddr_in srv;
  srv.sin_family = AF_INET;
  srv.sin_addr.s_addr = INADDR_ANY;
  srv.sin_port = htons(n->tcp_port);

  printf("TCP port assigned is %d\n", ntohs(srv.sin_port));

  if ((bind(tcp_sock, (const sockaddr *)&srv, sizeof(srv))) < 0)
  {
    printf("bind failure\n");
    exit(EXIT_FAILURE);
  }

  if (listen(tcp_sock, 1) < 0)
  {
    printf("bind failure\n");
    exit(EXIT_FAILURE);
  }

  n->tcp_socket_fd = tcp_sock;
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
      record_t fetched = fetch(msg.key);
      if (fetched.key == -1)
      {
        // handle error
        printf("error fetching...\n");
      }

      // open tcp connection and send
      int comm_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (comm_socket < 0)
      {
        printf("failed to create socket\n");
        exit(1);
      }
      sockaddr_in dest;
      dest.sin_addr.s_addr = msg.address;
      dest.sin_family = AF_INET;
      dest.sin_port = htons(msg.tcp_port);
      uint addrln = sizeof(struct sockaddr);

      if ((connect(comm_socket, (struct sockaddr *)&dest, addrln)) < 0)
      {
        printf("failed to connect to server\n");
        exit(1);
      }

      message_t *reply = newMessage(MESSAGE_TYPE_GET_REPLY_X, fetched.key, fetched.value, 0, 0);
      int bytes = send(comm_socket, reply, sizeof(message_t), 0);
      if (bytes < 0)
      {
        printf("failed to connect to server\n");
        exit(1);
      }

      printf("%d - %d fetched from store\n", fetched.key, fetched.value);
      break;
    case MESSAGE_TYPE_PUT_FORWARD:
      printf("SET VALUE\n");
      insert(msg.key, msg.value);
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
    int key = atoi(keystr);
    if (key == 0)
    {
      printf("Value cannot be 0 or a string\n");
      return;
    }
    if ((key % 3) + 1 == n.id)
    {
      printf("VALUE IN MY STORE\n");
      record_t rec = fetch(key);
      printf("Key: %d - Value: %d\n", rec.key, rec.value);
    }
    else
    {
      printf("VALUE IN A REMOTE STORE, FORWARD UDP MESSAGE\n");
      message_t *msg = newMessage(MESSAGE_TYPE_GET_FORWARD, key, 0, n.ip_addr, n.tcp_port);

      if (sendto(n.next_hop_udp_socket_fd, msg, sizeof(message_t), 0, (struct sockaddr *)&n.next_hop_addr, sizeof(n.next_hop_addr)) < 0)
      {
        printf("sendto()");
        exit(2);
      }
      // message has been forwarded on the UDP chain, wait for the destination server to initiate tcp comms.

      sockaddr_in srv;
      srv.sin_family = AF_INET;
      srv.sin_addr.s_addr = INADDR_ANY;
      srv.sin_port = htons(n.tcp_port);

      uint addrln = sizeof(struct sockaddr);

      printf("Waiting for TCP client to connect\n");
      int comm_socket = accept(n.tcp_socket_fd, (struct sockaddr *)&srv, &addrln);
      if (comm_socket < 0)
      {
        printf("accept()");
        exit(2);
      }

      message_t incoming_msg;
      int bytes = recv(comm_socket, (char *)&incoming_msg, sizeof(message_t), 0);
      if (bytes < 0)
      {
        printf("recv()");
        exit(2);
      }

      printf("TYPE: %d - Key: %d - Value: %d\n", incoming_msg.type, incoming_msg.key, incoming_msg.value);
      close(comm_socket);
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
      printf("VALUE BELONGS IN MY STORE\n");
      insert(key, value);
    }
    else
    {
      printf("VALUE BELONGS IN REMOTE STORE, FORWARD UDP MESSAGE\n");
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
  init_store();

  node_t n = new_node();
  udp_socket(&n);
  tcp_socket(&n);
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
