#include "lib.h"

using namespace std;

unordered_map<string, pair<int, bool>> clients;
unordered_map<string, vector<pair<int, uint8_t>>> subscriptions;
unordered_map<string, vector<tcp_server_message>> sf;

void handle_passive_tcp_sock(vector<struct pollfd> &poll)
{
  char buf[MESSAGE_SIZE];
  memset(buf, 0, sizeof(buf));

  int tcp_sock = poll[0].fd;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  /* Returns a new socket specific for the client */
  int client_sockfd = accept(tcp_sock, (sockaddr *)&client_addr, &client_len);
  DIE(client_sockfd < 0, "accept");

  mark_socket_as_reusable(client_sockfd);

  /* The number of clients exceeds the maximum conections number */
  DIE(clients.size() == SOMAXCONN, "too many clients");

  int rc = recv(client_sockfd, buf, CLIENT_ID_LEN, 0);
  DIE(rc < 0, "recv from new client");

  string client_id = string(buf);
  auto client = clients.find(client_id);
  if (client == clients.end())
  {
    /* The client is new */
    clients.insert({client_id, {client_sockfd, true}});

    /* Add the new client to the poll */
    poll.push_back({client_sockfd, POLLIN, 0});
    display_connection(client_addr, client_id);
    return;
  }
  bool connected = client->second.second;
  if (connected == true)
  {
    /* The client is already connected */
    cout << "Client " << client_id << " already connected." << endl;
    /* We can close the new socket */
    close(client_sockfd);
    return;
  }
  else if (connected == false)
  {
    /* The client is trying to reconnect */
    display_connection(client_addr, client_id);
    /* Update the client's socket */
    client->second.first = client_sockfd;
    /* Mark the client as connected again */
    client->second.second = true;

    /* Add the client to the poll */
    poll.push_back({client_sockfd, POLLIN, 0});

    string client_id = find_client(clients, client_sockfd);
    /* Send all the messages that were stored for the client */
    send_all_sf_messsages(sf, client_sockfd, client_id);
  }
}

int handle_stdin_server(vector<struct pollfd> &poll)
{
  int tcp_sock = poll[0].fd;
  int udp_sock = poll[1].fd;

  string user_input;
  cin >> user_input;

  if (user_input == "exit")
  {
    close(tcp_sock);
    close(udp_sock);

    return 1;
  }
  else
  {
    cout << "Invalid command." << endl;
    fflush(stdin);
  }

  return 0;
}

void handle_clients_sock(vector<struct pollfd> &poll)
{
  int rc;

  for (auto i = 3u; i < poll.size(); ++i)
  {
    if (poll[i].revents & POLLIN)
    {
      char buf[MESSAGE_SIZE];
      memset(buf, 0, sizeof(buf));

      int client_sockfd = poll[i].fd;
      rc = recv(client_sockfd, buf, sizeof(tcp_client_message), 0);
      DIE(rc < 0, "recv from client");

      /* If no data is received, the client has disconnected */
      if (rc == 0)
      {
        /* Remove the client from the poll */
        poll.erase(poll.begin() + i);
        /* Temporarily shutdown the socket */
        rc = close(client_sockfd);
        DIE(rc < 0, "close client socket");
        /* Mark the client as disconnected */
        string disco_id = find_client(clients, client_sockfd);
        clients[disco_id].second = false;

        cout << "Client " << disco_id << " disconnected." << endl;
        continue;
      }

      tcp_client_message *msg = (tcp_client_message *)buf;

      if (strncmp(msg->cmd, "subscribe", 9) == 0)
      {
        /* Add the client to the list of subscribers */
        const string topic = string(msg->topic);
        uint8_t sf = msg->sf;
        subscriptions[topic].push_back({client_sockfd, sf});
      }

      else if (strncmp(msg->cmd, "unsubscribe", 11) == 0)
      {
        /* Remove the client from the list of subscribers */
        const string topic = string(msg->topic);
        remove_subscription(subscriptions, topic, client_sockfd);
      }

      else
      {
        cout << "Received an invalid command from the TCP client." << endl;
      }
    }
  }
}

void handle_udp_sock(vector<struct pollfd> &poll)
{
  char buf[MESSAGE_SIZE];
  memset(buf, 0, sizeof(buf));

  int udp_sock = poll[1].fd;

  struct sockaddr_in udp_client;
  socklen_t udp_client_len = sizeof(udp_client);

  int rc = recvfrom(udp_sock, buf, sizeof(udp_message), 0, (sockaddr *)&udp_client, &udp_client_len);
  DIE(rc < 0, "recv from udp socket");

  udp_message *udp_msg = (udp_message *)buf;
  tcp_server_message *tcp_msg = parse_udp_message(udp_msg, udp_client);

  /* Send the message to all the subscribers */
  send_to_subscribers(subscriptions, clients, sf, tcp_msg, string(udp_msg->topic));
}

void run_server(int tcp_sock, int udp_sock)
{
  vector<struct pollfd> ready_socks(3, pollfd());

  ready_socks[0].fd = tcp_sock;   /* Listening socket */
  ready_socks[0].events = POLLIN; /* POLLIN marks that there is data to read */

  ready_socks[1].fd = udp_sock; /* UDP socket*/
  ready_socks[1].events = POLLIN;

  ready_socks[2].fd = STDIN_FILENO; /* STDIN FD */
  ready_socks[2].events = POLLIN;

  while (true)
  {
    fflush(stdin);

    int ready = poll(ready_socks.data(), ready_socks.size(), -1);
    DIE(ready < 0, "poll sockets");

    DIE(ready == 0, "timeout!");

    if (ready_socks[0].revents & POLLIN)
    {
      /* There is a new connection request on the listening socket */
      handle_passive_tcp_sock(ready_socks);
    }
    else if (ready_socks[1].revents & POLLIN)
    {
      /* There is data on the udp socket */
      handle_udp_sock(ready_socks);
    }
    else if (ready_socks[2].revents & POLLIN)
    {
      /* There is data on the stdin */
      if (handle_stdin_server(ready_socks) == 1)
      {
        exit(0);
      }
    }
    else
    {
      /* There is data on the client`s socket */
      handle_clients_sock(ready_socks);
    }
  }
}

/* Server has only 1 argument , being the port */
int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    cerr << "Usage: " << argv[0] << " ./server <port>" << endl;
    exit(1);
  }

  int rc, ngl;

  /* Create sockets for TCP and UDP */
  int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcp_sockfd < 0, "tcp socket");

  int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udp_sockfd < 0, "udp socket");

  mark_socket_as_reusable(tcp_sockfd);
  mark_socket_as_reusable(udp_sockfd);

  /* Get the port number from argv */
  int port = atoi(argv[1]);

  struct sockaddr_in tcp_addr = setup_socket(port);
  struct sockaddr_in udp_addr = setup_socket(port);

  rc = setsockopt(udp_sockfd, SOL_SOCKET, TCP_NODELAY, (char *)&ngl, sizeof(int));
  DIE(rc < 0, "setsockopt");

  /* Bind the TCP socket */
  rc = bind(tcp_sockfd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr));
  DIE(rc < 0, "tcp bind");

  /* Bind the UDP socket */
  rc = bind(udp_sockfd, (struct sockaddr *)&udp_addr, sizeof(tcp_addr));
  DIE(rc < 0, "udp bind");

  /* Listen for incoming connections */
  rc = listen(tcp_sockfd, SOMAXCONN);
  DIE(rc < 0, "tcp listen");

  run_server(tcp_sockfd, udp_sockfd);

  close(tcp_sockfd); /* Close the TCP listening socket */
  close(udp_sockfd); /* Close the UDP socket */

  return 0;
}
