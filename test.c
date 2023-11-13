#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char const *argv[])
{
  int msfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (msfd < 0)
  {
    printf("error initiating master socket");
    exit(EXIT_FAILURE);
  }

  sockaddr_in remote;
  remote.sin_addr.s_addr = inet_addr("127.0.0.1");
  remote.sin_family = AF_INET;
  remote.sin_port = htons(8080);

  message_t *msg = newMessage(MESSAGE_TYPE_WHAT_X, 18, 63, 1000, 1863);

  /* Send the message in buf to the server */
  if (sendto(msfd, msg, sizeof(message_t), 0, (struct sockaddr *)&remote, sizeof(remote)) < 0)
  {
    printf("sendto()");
    exit(2);
  }

  /* Deallocate the socket */
  close(msfd);
}