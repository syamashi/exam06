#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_client {
  int fd, id;
  struct s_client *next;
} t_client;

t_client *g_client;
int sockfd, g_id;
fd_set cur_sock, write_fds, read_fds;
char msg[100], tmp[1 << 18], str[1 << 18], buf[100 + (1 << 18)];

void fatal() {
  write(2, "Fatal error\n", strlen("Fatal error\n"));
  close(sockfd);
  exit(1);
}

int get_id(int fd) {
  t_client *ptr = g_client;
  while (ptr) {
    if (ptr->fd == fd) return ptr->id;
    ptr = ptr->next;
  }
  return -1;
}

int get_max_fd() {
  t_client *ptr = g_client;
  int mx = sockfd;
  while (ptr) {
    if (ptr->fd > mx) mx = ptr->fd;
    ptr = ptr->next;
  }
  return mx;
}

void add_list(int fd) {
  t_client *new = (t_client *)calloc(1, sizeof(t_client));
  if (!new) {
    fatal();
  }
  new->fd = fd;
  new->id = g_id++;
  if (g_client == NULL) {
    g_client = new;
  } else {
    t_client *ptr = g_client;
    while (ptr->next) {
      ptr = ptr->next;
    }
    ptr->next = new;
  }
}

void remove_list(int fd) {
  t_client *del;
  if (!g_client) return;
  if (g_client->fd == fd) {
    del = g_client;
    g_client = g_client->next;
    free(del);
    return;
  }
  t_client *ptr = g_client;
  while (ptr->next && ptr->next->fd != fd) {
    ptr = ptr->next;
  }
  del = ptr->next;
  ptr->next = del->next;
  free(del);
}

void send_all(int fd, char *ms) {
  t_client *ptr = g_client;
  while (ptr) {
    if (ptr->fd != fd && FD_ISSET(ptr->fd, &write_fds)) {
      if (send(ptr->fd, ms, strlen(ms), 0) < 0) fatal();
    }
    ptr = ptr->next;
  }
}

int extract_message(int fd) {
  int i = 0;
  int j = 0;
  while (str[i]) {
    tmp[j++] = str[i];
    if (str[i] == '\n') {
      sprintf(buf, "client %d: %s", get_id(fd), tmp);
      send_all(fd, buf);
      bzero(&tmp, strlen(tmp));
      bzero(&buf, strlen(buf));
      j = 0;
    }
    ++i;
  }
}

void add_client() {
  int connfd;
  struct sockaddr_in cli;
  socklen_t len = sizeof(cli);
  connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
  if (connfd < 0) {
    fatal();
  }
  add_list(connfd);
  sprintf(msg, "server: client %d just arrived\n", get_id(connfd));
  send_all(connfd, msg);
  FD_SET(connfd, &cur_sock);
  bzero(&msg, strlen(msg));
}

void remove_client(int fd) {
  sprintf(msg, "server: client %d just left\n", get_id(fd));
  remove_list(fd);
  send_all(fd, msg);
  FD_CLR(fd, &cur_sock);
  close(fd);
  bzero(&msg, strlen(msg));
}

int main(int ac, char **av) {
  if (ac != 2) {
    write(2, "Wrong number of arguments\n",
          strlen("Wrong number of arguments\n"));
    exit(1);
  }

  struct sockaddr_in servaddr;

  bzero(&servaddr, sizeof(servaddr));
  uint16_t port = atoi(av[1]);

  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1
  servaddr.sin_port = htons(port);

  // socket create and verification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    fatal();
  }
  // Binding newly created socket to given IP and verification
  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) !=
      0) {
    fatal();
  }
  if (listen(sockfd, 0) != 0) {
    fatal();
  }
  FD_ZERO(&cur_sock);
  FD_SET(sockfd, &cur_sock);
  while (1) {
    read_fds = write_fds = cur_sock;
    int MX = get_max_fd();
    if (select(MX + 1, &read_fds, &write_fds, NULL, NULL) < 0) continue;
    for (int fd = 0; fd <= MX; ++fd) {
      if (FD_ISSET(fd, &read_fds)) {
        if (fd == sockfd) {
          add_client();
          break;
        }
        int recv_cnt = 1000;
        int slen = 0;
        while (recv_cnt == 1000 || str[slen - 1] != '\n') {
          recv_cnt = recv(fd, str + slen, 1000, 0);
          slen += recv_cnt;
          if (recv_cnt <= 0) break;
        }
        if (recv_cnt <= 0) {
          remove_client(fd);
          break;
        }
        extract_message(fd);
        bzero(&str, strlen(str));
      }
    }
  }
}