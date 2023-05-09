#include "lib.h"

sockaddr_in setup_socket(int port)
{
  struct sockaddr_in sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;         /* IPv4 address family */
  sock_addr.sin_port = htons(port);       /* Port number */
  sock_addr.sin_addr.s_addr = INADDR_ANY; /* Accept any incoming messages */

  return sock_addr;
}

sockaddr_in setup_socket_client(int port, std::string ip_server)
{
  struct sockaddr_in sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;                             /*IPv4 address family*/
  sock_addr.sin_port = htons(port);                           /*Port number*/
  inet_pton(AF_INET, ip_server.c_str(), &sock_addr.sin_addr); /*IP address*/

  return sock_addr;
}

void mark_socket_as_reusable(int sockfd)
{
  int8_t flag = 1;
  int rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
  DIE(rc < 0, "setsockopt");
}

void disable_nagle(int sockfd)
{
  int8_t flag = 1;
  int rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
  DIE(rc < 0, "setsockopt");
}

void display_connection(struct sockaddr_in client, std::string client_id)
{
  std::cout << "New client " << client_id << " connected from ";
  std::cout << inet_ntoa(client.sin_addr) << ":";
  std::cout << ntohs(client.sin_port) << "." << std::endl;
}

std::string find_client(std::unordered_map<std::string, std::pair<int, bool>> &clients, int sockfd)
{
  for (auto it = clients.begin(); it != clients.end(); ++it)
  {
    if (it->second.first == sockfd)
    {
      return it->first;
    }
  }
  return "";
}

void remove_subscription(std::unordered_map<std::string, std::vector<std::pair<int, uint8_t>>> &subscriptions,
                         std::string topic, int sockfd)
{
  for (auto it = subscriptions[topic].begin(); it != subscriptions[topic].end(); ++it)
  {
    if (it->first == sockfd)
    {
      subscriptions[topic].erase(it);
      break;
    }
  }
}

std::vector<std::string> split_string(std::string str, const std::string delimiter)
{
  std::vector<std::string> result;
  size_t pos = 0;
  std::string token;
  while ((pos = str.find(delimiter)) != std::string::npos)
  {
    token = str.substr(0, pos);
    result.push_back(token);
    str.erase(0, pos + delimiter.length());
  }
  result.push_back(str);
  return result;
}

tcp_server_message *parse_udp_message(udp_message *udp, sockaddr_in client)
{
  tcp_server_message *tcp = (tcp_server_message *)malloc(sizeof(tcp_server_message));

  tcp->type = udp->type;
  tcp->udp_client = client;
  strncpy(tcp->topic, udp->topic, TOPIC_LEN);

  switch ((int)udp->type)
  {
  case 0:
  {
    uint8_t sign;
    int nr_1;

    memcpy(&sign, udp->payload, sizeof(uint8_t));
    memcpy(&nr_1, udp->payload + sizeof(uint8_t), sizeof(int));

    nr_1 = ntohl(nr_1);
    nr_1 = (sign == 0) ? nr_1 : nr_1 * (-1);

    memcpy(tcp->payload, &nr_1, sizeof(int));
    break;
  }
  case 1:
  {
    double nr_2;

    nr_2 = (*(uint16_t *)udp->payload);

    nr_2 = ntohs(nr_2);
    nr_2 /= 100;

    memcpy(tcp->payload, &nr_2, sizeof(double));
    break;
  }
  case 2:
  {
    uint8_t sign, u_pow;
    float nr_3;

    memcpy(&sign, udp->payload, sizeof(uint8_t));
    nr_3 = (*(uint32_t *)(udp->payload + sizeof(uint8_t)));
    memcpy(&u_pow, udp->payload + sizeof(uint8_t) + sizeof(float), sizeof(uint8_t));

    nr_3 = ntohl(nr_3);
    nr_3 /= std::pow(10, u_pow);
    nr_3 = (sign == 0) ? nr_3 : nr_3 * (-1);

    memcpy(tcp->payload, &nr_3, sizeof(float));
    break;
  }
  case 3:
  {
    int len = strlen(udp->payload);

    memcpy(tcp->payload, udp->payload, len);

    tcp->payload[len] = '\0';
    break;
  }
  default:
    DIE(true, "invalid type");
  }

  return tcp;
}

void send_to_subscribers(std::unordered_map<std::string, std::vector<std::pair<int, uint8_t>>> &subscriptions,
                         std::unordered_map<std::string, std::pair<int, bool>> &clients,
                         std::unordered_map<std::string, std::vector<struct tcp_server_message>> &sf,
                         tcp_server_message *tcp, std::string topic)
{
  for (auto it = subscriptions[topic].begin(); it != subscriptions[topic].end(); ++it)
  {
    std::string id = find_client(clients, it->first);
    if (it->second == 0)
    {
      /* Store and forward flag is not set */
      if (clients[id].second == true)
      {
        /* The client is online , so we send the message */
        int rc = send(it->first, (char *)tcp, sizeof(tcp_server_message), 0);
        DIE(rc < 0, "send");
      }
    }
    else
    {
      /* Store and forward flag is set */
      if (clients[id].second == true)
      {
        /* The client is online , so we send the message */
        int rc = send(it->first, (char *)tcp, sizeof(tcp_server_message), 0);
        DIE(rc < 0, "send");
      }
      else
      {
        /* The client is offline , so we store the message */
        sf[id].push_back(*tcp);
      }
    }
  }
}

void send_all_sf_messsages(std::unordered_map<std::string, std::vector<struct tcp_server_message>> &sf,
                           int client, std::string id)
{
  for (auto it = sf[id].begin(); it != sf[id].end(); ++it)
  {
    int rc = send(client, (char *)&(*it), sizeof(tcp_server_message), 0);
    DIE(rc < 0, "send");
  }
  sf[id].clear();
}

std::string format_float(float value, int precision)
{
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(precision) << value;
  return ss.str();
}

std::string format_double(double value, int precision)
{
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(precision) << value;
  return ss.str();
}

void display_client_message(tcp_server_message *msg)
{
  struct sockaddr_in udp_client = msg->udp_client;
  std::string data_type;
  std::string payload;
  switch (msg->type)
  {
  case 0:
    data_type = "INT";
    payload = std::to_string(*(int *)msg->payload);
    break;
  case 1:
    data_type = "SHORT_REAL";
    payload = format_double(*(double *)msg->payload, 2);
    break;
  case 2:
    data_type = "FLOAT";
    payload = format_float(*(float *)msg->payload, 4);
    break;
  case 3:
    data_type = "STRING";
    payload = std::string(msg->payload);
    break;
  };

  std::cout << inet_ntoa(udp_client.sin_addr) << ":";
  std::cout << ntohs(udp_client.sin_port) << " - ";
  std::cout << msg->topic << " - ";
  std::cout << data_type << " - ";
  std::cout << payload << std::endl;
}
