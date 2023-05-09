#include "lib.h"

using namespace std;

int handle_client_stdin(int tcp_sock)
{
  string user_input;
  getline(cin, user_input);

  if (user_input == "exit")
  {
    return 1;
  }

  vector<string> tokens = split_string(user_input, " ");
  tcp_client_message msg;

  if (tokens[0] == "subscribe" && tokens.size() == 3)
  {
    cout << "Subscribed to topic." << endl;

    memcpy(msg.cmd, tokens[0].c_str(), tokens[0].length() + 1);
    memcpy(msg.topic, tokens[1].c_str(), tokens[1].length() + 1);
    msg.sf = stoi(tokens[2]);

    int rc = send(tcp_sock, (char *)&msg, sizeof(tcp_client_message), 0);
    DIE(rc < 0, "send subscribe to server");
  }

  else if (tokens[0] == "unsubscribe" && tokens.size() == 2)
  {
    cout << "Unsubscribed from topic." << endl;

    memcpy(msg.cmd, tokens[0].c_str(), tokens[0].length() + 1);
    memcpy(msg.topic, tokens[1].c_str(), tokens[1].length() + 1);

    int rc = send(tcp_sock, (char *)&msg, sizeof(tcp_client_message), 0);
    DIE(rc < 0, "send unsubscribe to server");
  }

  else
  {
    cout << "Invalid command" << endl;
    fflush(stdin);
  }

  return 0;
}

int handle_tcp_sock(int tcp_sock)
{
  int rc;
  char buf[MESSAGE_SIZE];
  memset(buf, 0, sizeof(buf));

  rc = recv(tcp_sock, buf, sizeof(tcp_server_message), 0);
  DIE(rc < 0, "recv from server");

  /* If no data is received, the server has closed */
  if (rc == 0)
  {
    cout << "Server stopped." << endl;
    return 1;
  }

  tcp_server_message *msg = (tcp_server_message *)buf;
  display_client_message(msg);

  return 0;
}

void run_client(int tcp_sock)
{
  struct pollfd ready_socks[2];

  ready_socks[0].fd = tcp_sock;   /* Listening socket */
  ready_socks[0].events = POLLIN; /* POLLIN marks that there is data to read */

  ready_socks[1].fd = STDIN_FILENO; /* STDIN */
  ready_socks[1].events = POLLIN;

  while (true)
  {
    int ready = poll(ready_socks, sizeof(ready_socks) / sizeof(struct pollfd), -1);
    DIE(ready < 0, "poll sockets");

    DIE(ready == 0, "timeout!");

    if (ready_socks[0].revents & POLLIN)
    {
      /* There is data on the tcp socket */
      if (handle_tcp_sock(tcp_sock) == 1)
        exit(0);
    }
    else if (ready_socks[1].revents & POLLIN)
    {
      /* There is data on the stdin */
      if (handle_client_stdin(tcp_sock) == 1)
        exit(0);
    }
  }
}

/* Subscriber has 3 arguments : 1 - ID Client, 2 - IP Server, 3 - Port Server*/
int main(int argc, char *argv[])
{
  if (argc < 4)
  {
    cerr << "Usage: " << argv[0] << " ./subscriber <ID_Client> <IP_Server> <Port_Server>" << endl;
    exit(1);
  }

  int rc;

  int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcp_sockfd < 0, "tcp socket");

  disable_nagle(tcp_sockfd);

  const string id_client = argv[1];
  const string ip_server = argv[2];
  int port = atoi(argv[3]);

  struct sockaddr_in serv_addr = setup_socket_client(port, ip_server);

  /*Connect to the internet*/
  rc = connect(tcp_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "tcp connect");

  /*Send the client id to the server*/
  rc = send(tcp_sockfd, id_client.c_str(), id_client.length() + 1, 0);
  DIE(rc < 0, "tcp send");

  run_client(tcp_sockfd);

  close(tcp_sockfd); /*Close the TCP socket*/

  return 0;
}
