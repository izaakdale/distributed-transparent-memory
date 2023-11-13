#include <arpa/inet.h>

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

typedef struct node
{
  unsigned int id;
  unsigned int ip_addr;
  unsigned int udp_port;
  unsigned int tcp_port;
  unsigned int next_hop_ip;
  unsigned int next_hop_udp_port;
} node_t;

typedef enum
{
  MESSAGE_TYPE_PUT_FORWARD,
  MESSAGE_TYPE_GET_FORWARD,
  MESSAGE_TYPE_WHAT_X,
  MESSAGE_TYPE_PUT_REPLY_X,
  MESSAGE_TYPE_GET_REPLY_X,
} MESSAGE_TYPE_T;

typedef struct message
{
  MESSAGE_TYPE_T type;

  int key;
  int value;

  unsigned int address;
  unsigned int tcp_port;

} message_t;

message_t *newMessage(MESSAGE_TYPE_T type, int key, int value, unsigned int address, unsigned int tcp_port)
{
  struct message *m = (struct message *)malloc(sizeof(struct message));
  m->type = type;
  m->key = key;
  m->value = value;
  m->address = address;
  m->tcp_port = tcp_port;
  return m;
}
