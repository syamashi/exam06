#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

typedef struct s_client{
  int fd;
  int id;
  struct s_client *next;
} t_client;

t_client *g_client = NULL;
char buf[42 * (1 << 12)], tmp[42 * (1 << 12)], str[42 * (1 << 12) + 42];
char msg[42];
int sock_fd, g_id;
fd_set cur_sock, cpy_read, cpy_write;

void fatal(){
  write(2, "Fatal error\n", strlen("Fatal error\n"));
  close(sock_fd);
  exit(1);
}

int get_max_fd(){
  int max = sock_fd;
  t_client *ptr = g_client;
  while(ptr){
    if (ptr->id > max) max = ptr->id;
    ptr = ptr->next;
  }
  return max;
}

void send_all(int fd, char *str_req){
  t_client *ptr = g_client;

  while(ptr){
    if (ptr->fd != fd && FD_ISSET(ptr->fd, &cpy_write)){
      if (send(ptr->fd, str_req, strlen(str_req), 0) < 0) fatal();
    }
    ptr = ptr->next;
  }
}

int add_client_to_list(int fd){
  t_client *ptr = g_client;
  t_client *new;

  if (!(new = calloc(1, sizeof(t_client)))) fatal();
  new->id = g_id++;
  new->fd = fd;
  if (!g_client) g_client = new;
  else{
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

  if ((client_fd = accept(sock_fd, (struct sockaddr *)&clientaddr, &len)) < 0) fatal();
  sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
  send_all(client_fd, msg);
  FD_SET(client_fd, &cur_sock);
}

int get_id(int fd){
  t_client *ptr = g_client;
  while(ptr){
    if (ptr->fd == fd) return ptr->id;
    ptr = ptr->next;
  }
  return -1;
}

int rm_client(int fd){
  t_client *ptr = g_client;
  t_client *del;
  t_client *prev;
  int id = get_id(fd);
  
  if (ptr && ptr->fd == fd){
    g_client = ptr->next;
    free(ptr);
  }
  else{
    while (ptr && ptr->next && ptr->next->fd != fd){
      ptr = ptr->next;
    }
    del = ptr->next;
    ptr->next = ptr->next->next;
    free(del);
  }
  return id;
}

void ex_msg(int fd){
  int i = 0;
  int j = 0;
  while(str[i]){
    tmp[j] = str[i];
    ++j;
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
  uint16_t port = atoi(av[1]);
	bzero(&servaddr, sizeof(servaddr)); 
	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

  if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) fatal();
  if ((bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) fatal();
  if (listen(sock_fd, 0) != 0) fatal();
  
  FD_ZERO(&cur_sock);
  FD_SET(sock_fd, &cur_sock);
  while(1){
    cpy_write = cpy_read = cur_sock;
    int MAX_FD = get_max_fd();
    if (select(MAX_FD + 1, &cpy_read, &cpy_write, NULL, NULL) < 0){
      continue;
    }
    for (int fd = 0; fd <= MAX_FD; ++fd){
      if (FD_ISSET(fd, &cpy_read)){
        if (fd == sock_fd){
          bzero(&msg, sizeof(msg));
          add_client();
          break;
        }
        else{
          int ret_recv = 1000;
          while (ret_recv == 1000 || str[strlen(str) - 1] != '\n'){
            ret_recv = recv(fd, str + strlen(str), 1000, 0);
            if (ret_recv <= 0) break;
          }
          if (ret_recv <= 0){
            bzero(&msg, sizeof(msg));
            sprintf(msg, "server: client %d just left\n", rm_client(fd));
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