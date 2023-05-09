#ifndef _LIB_H
#define _LIB_H

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>

#pragma once
#include "structures.h"

#define MESSAGE_SIZE 4096

/* DIE macro */
#define DIE(condition, message, ...)                                                   \
	do                                                                                   \
	{                                                                                    \
		if ((condition))                                                                   \
		{                                                                                  \
			fprintf(stderr, "[(%s:%d)]: " #message "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
			perror("");                                                                      \
			exit(1);                                                                         \
		}                                                                                  \
	} while (0)

sockaddr_in setup_socket(int port);
sockaddr_in setup_socket_client(int port, std::string ip_server);
void mark_socket_as_reusable(int sockfd);
void disable_nagle(int sockfd);
void display_connection(struct sockaddr_in client, std::string client_id);
std::string find_client(std::unordered_map<std::string, std::pair<int, bool>> &clients, int sockfd);
void remove_subscription(std::unordered_map<std::string, std::vector<std::pair<int, uint8_t>>> &subscriptions,
												 std::string topic, int sockfd);
std::vector<std::string> split_string(std::string str, const std::string delimiter);
tcp_server_message *parse_udp_message(udp_message *udp, sockaddr_in client);
void send_to_subscribers(std::unordered_map<std::string, std::vector<std::pair<int, uint8_t>>> &subscriptions,
												 std::unordered_map<std::string, std::pair<int, bool>> &clients,
												 std::unordered_map<std::string, std::vector<struct tcp_server_message>> &sf,
												 tcp_server_message *tcp, std::string topic);
void send_all_sf_messsages(std::unordered_map<std::string, std::vector<struct tcp_server_message>> &sf,
													 int client, std::string id);
void display_client_message(tcp_server_message *msg);

#endif /* _LIB_H */