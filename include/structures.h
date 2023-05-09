#ifndef _STRUCTURES_H
#define _STRUCTURES_H

#pragma once
#include "lib.h"

#define TOPIC_LEN 50
#define PAYLOAD_LEN 1500
#define TIMEOUT 10000
#define CLIENT_ID_LEN 10
#define CMD_LEN 12

/* Message sent by a UDP client */

struct udp_message
{
  char topic[TOPIC_LEN];
  uint8_t type;
  char payload[PAYLOAD_LEN];
};

typedef struct udp_message udp_message;

/* Message sent by a TCP client */

struct tcp_client_message
{
  char cmd[CMD_LEN];
  char topic[TOPIC_LEN];
  uint8_t sf;
};

typedef struct tcp_client_message tcp_client_message;

struct tcp_server_message
{
  struct sockaddr_in udp_client;
  char topic[TOPIC_LEN];
  uint8_t type;
  char payload[PAYLOAD_LEN];
};

typedef struct tcp_server_message tcp_server_message;

#endif /* _STRUCTURES_H */