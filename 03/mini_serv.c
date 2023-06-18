#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

typedef struct s_client{
  int fd, id;
  struct s_client *next;
} t_client;

int sockfd, g_id;
t_client *g_client;
char str[1<<18], tmp[1<<18], buf[100 + 1<<18], msg[100];
fd_set cur_sock, cpy_read, cpy_write;

void fatal(){
  write(2, "Fatal error\n", strlen("Fatal error\n"));
  close(sockfd);
  exit(1);
}

void send_all(int fd, char *ms){
  t_client *ptr = g_client;
  while (ptr){
    if (ptr->fd != fd && FD_ISSET(ptr->fd, &cpy_write)){
      if (send(ptr->fd, ms, strlen(ms), 0) < 0) fatal();
    }
    ptr = ptr->next;
  }
}

int get_id(int fd){
  t_client *ptr = g_client;
  while (ptr){
    if (ptr->fd == fd) return ptr->id;
    ptr = ptr->next;
  }
  return -1;
}

int get_max_fd(){
  int mx = sockfd;
  t_client *ptr = g_client;
  while (ptr){
    if (ptr->fd > mx) mx = ptr->fd;
    ptr = ptr->next;
  }
  return mx;
}

void extract_message(int fd)
{
	int	i = 0;
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

int remove_fd(int fd){
  int ret_id = 0;
  t_client *del;
  if (g_client->fd == fd){
    del = g_client;
    g_client = g_client->next;
    ret_id = del->id;
    free(del);
  }
  else{
    t_client *ptr = g_client;
    while(ptr->next && ptr->next->fd != fd) ptr = ptr->next;
    del = ptr->next;
    ptr->next = del->next;
    ret_id = del->id;
    free(del);
  }
  return ret_id;
}
int add_list_client(int fd){
  int ret_id = g_id++;
  t_client *new = (t_client*)calloc(1, sizeof(t_client));
  if (new == NULL) fatal();
  new->id = ret_id;
  new->fd = fd;
  if (g_client == NULL){
    g_client = new;
  }
  else{
    t_client *ptr = g_client;
    while(ptr->next) ptr = ptr->next;
    ptr->next = new;
  }
  return ret_id;
}

void accept_client(){
	struct sockaddr_in cli; 
  socklen_t len;
  int connfd;
	len = sizeof(cli);
	connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0) {
    fatal();
  }
  sprintf(msg, "server: client %d just arrived\n", add_list_client(connfd));
  send_all(connfd, msg);
  FD_SET(connfd, &cur_sock);
  bzero(&msg, strlen(msg));
}

int main(int ac, char **av) {
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

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) fatal();
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
    fatal();
	} 
	if (listen(sockfd, 0) != 0) {
    fatal();
	}
  FD_ZERO(&cur_sock);
  FD_SET(sockfd, &cur_sock);
  while(1){
    cpy_read = cpy_write = cur_sock;
    int MX = get_max_fd();
    if (select(MX + 1, &cpy_read, &cpy_write, NULL, NULL) < 0) continue;
    for(int fd = 0; fd <= MX; ++fd){
      if (FD_ISSET(fd, &cpy_read)){
        if (fd == sockfd){
          accept_client();
          break;
        }
        int recv_cnt = 1000;
        while(recv_cnt == 1000 || str[strlen(str) - 1] != '\n'){
          recv_cnt = recv(fd, str + strlen(str), 1000, 0);
          if (recv_cnt <= 0) break;
        }
        if (recv_cnt <= 0){
          sprintf(msg, "server: client %d just left\n", remove_fd(fd));
          send_all(fd, msg);
          FD_CLR(fd, &cur_sock);
          close(fd);
          bzero(&msg, strlen(msg));
          break;
        }
        else{
          extract_message(fd);
        }
      }
    }
  }
}