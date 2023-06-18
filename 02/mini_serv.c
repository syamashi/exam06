#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>

int sockfd, g_id;
fd_set cur_sock, cpy_read, cpy_write;

typedef struct s_client{
  int fd;
  int id;
  struct s_client *next;
} t_client;

t_client *g_clients = NULL;

char msg[42];
char str[1<<18], buf[(1<<18) + 100], tmp[1<<18];

void fatal(){
  write(2, "Fatal error\n", strlen("Fatal error\n"));
  close(sockfd);
  exit(1);
}

int remove_fd(int fd){
  t_client *ptr = g_clients;
  t_client *del;
  int ret = -1;
  if (ptr->fd == fd){
    del = g_clients;
    g_clients = ptr->next;
    ret = ptr->id;
    free(del);
  }
  else{
    while(ptr->next && ptr->next->fd != fd){
      ptr = ptr->next;
    }
    del = ptr->next;
    ret = ptr->next->id;
    ptr->next = del->next;
    free(del);
  }
  return ret;
}

int get_id(int fd){
  t_client *ptr = g_clients;
  while(ptr){
    if (ptr->fd == fd) return ptr->id;
    ptr = ptr->next;
  }
  return -1;
}

int get_max_fd(){
  int max = sockfd;
  t_client *ptr = g_clients;
  while(ptr){
    if (ptr->fd > max) max = ptr->fd;
    ptr = ptr->next;
  }
  return max;
}

void send_all(int fd, char *_msg){
  t_client *ptr = g_clients;
  while(ptr){
    if (ptr->fd != fd && FD_ISSET(ptr->fd, &cpy_write)){
      if (send(ptr->fd, _msg, strlen(_msg), 0) < 0) fatal();
    }
    ptr = ptr->next;
  }
}

int add_client_to_list(int fd){
  t_client *new;

  if ((new = calloc(1, sizeof(t_client))) == NULL) fatal();
  new->fd = fd;
  new->id = g_id++;
  if (g_clients == NULL){
    g_clients = new;
  }
  else{
    t_client *ptr = g_clients;
    while(ptr->next){
      ptr = ptr->next;
    }
    ptr->next = new;
  }
  return new->id;
}

void add_client(){
	struct sockaddr_in clientaddr;
  socklen_t len = sizeof(clientaddr);
  int client_fd;
	if((client_fd = accept(sockfd, (struct sockaddr *)&clientaddr, &len)) < 0) fatal();
  sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
  send_all(client_fd, msg);
  FD_SET(client_fd, &cur_sock);
}

void ex_msg(int fd){
  int i = 0;
  int j = 0;

  while(str[i]){
    tmp[j++] = str[i];
    if (str[i] == '\n'){
      sprintf(buf, "client %d: %s", get_id(fd), tmp);
      send_all(fd, buf);
      bzero(&buf, strlen(buf));
      bzero(&tmp, strlen(tmp));
      j = 0;
    }
    ++i;
  }
  bzero(&str, strlen(str));
}

int main(int ac, char **av){
  if (ac != 2){
    write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
    exit(1);
  }
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));

  uint16_t port = atoi(av[1]);
	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);
  
	// socket create and verification
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) fatal();
  if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) fatal();
	if (listen(sockfd, 0) != 0) fatal();

  FD_ZERO(&cur_sock);
  FD_SET(sockfd, &cur_sock);
  while(1){
    cpy_write = cpy_read = cur_sock;
    int MAX = get_max_fd();
    if (select(MAX + 1, &cpy_read, &cpy_write, NULL, NULL) < 0) continue;
    for(int fd = 0; fd <= MAX; ++fd){
      if (FD_ISSET(fd, &cpy_read)){
        if (fd == sockfd){
          bzero(&msg, strlen(msg));
          add_client();
          break;
        }
        else{
          int recv_cnt = 1000;
          while (recv_cnt == 1000 || str[strlen(str) - 1] != '\n'){
            recv_cnt = recv(fd, str + strlen(str), 1000, 0);
            if (recv_cnt <= 0) break;
          }
          if (recv_cnt <= 0){
            bzero(&msg, strlen(msg));
            sprintf(msg, "server: client %d just left\n", remove_fd(fd));
            send_all(fd, msg);
            FD_CLR(fd, &cur_sock);
            close(fd);
            break;
          }
          else{
            ex_msg(fd);
          }
        }
      }
    }
  }
  return 0;
}