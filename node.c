#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "common.h"

unsigned int n_of_nodes = 3;

unsigned int udp_socket()
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
  srv.sin_port = htons(8080);

  printf("Port assigned is %d\n", ntohs(srv.sin_port));

  if ((bind(udp_sock, (const sockaddr *)&srv, sizeof(srv))) < 0)
  {
    printf("bind failure\n");
    exit(EXIT_FAILURE);
  }

  // close(udp_sock);
  return udp_sock;
}

void processMessage(unsigned int sock_fd)
{
  message_t msg;
  sockaddr_in cli;
  unsigned int client_address_size = sizeof(cli);
  if (recvfrom(sock_fd, &msg, sizeof(message_t), 0, (struct sockaddr *)&cli, &client_address_size) < 0)
  {
    printf("recvfrom()");
    exit(4);
  }

  printf("Received message %u\n", msg.tcp_port);
}

void read_from_stdin()
{

  fd_set fdr;
  FD_SET(0, &fdr); // add STDIN to the fd set
  while (1)
  {
    select(1, &fdr, NULL, NULL, NULL);
    char buf[BUFSIZ];
    if (FD_ISSET(0, &fdr))
    {
      size_t count = read(0, buf, sizeof buf);
      printf("%s", buf);
      memset(buf, '\0', sizeof(buf));
    }
  }
}

int main(int argc, char const *argv[])
{
  // unsigned int n_of_nodes;
  // printf("How many nodes will be in your cluster? ");
  // scanf("%d", &n_of_nodes);

  node_t n;
  printf("IP addr : ");
  char ip_addr;
  scanf("%s", &ip_addr);
  n.ip_addr = inet_addr(&ip_addr);
  // for now the ip is the same
  n.next_hop_ip = inet_addr(&ip_addr);

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

  unsigned int udp_sock = udp_socket();

  fd_set fdr;
  while (1)
  {
    FD_ZERO(&fdr);
    FD_SET(0, &fdr); // add STDIN to the fd set
    FD_SET(udp_sock, &fdr);

    select(udp_sock + 1, &fdr, NULL, NULL, NULL);
    char buf[BUFSIZ];
    memset(buf, '\0', BUFSIZ);
    if (FD_ISSET(0, &fdr))
    {
      size_t count = read(0, buf, BUFSIZ);
      printf("%s", buf);
    }

    if (FD_ISSET(udp_sock, &fdr))
    {
      processMessage(udp_sock);
    }
  }

  return 0;
}
