#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include "datastruct.h"

//#define MAXLINE		1024
#define MAX_N_CHILD	4
#define SERV_PORT   5193 



int bind_socket(int sockfd, struct sockaddr_in addr);

int is_pkt_loss(int prob);

int sim_sendto(int socketd, char *msg, unsigned long len, struct sockaddr* addr, socklen_t addr_len, int p_loss);


int getACKseqnum(int sockfd, char* buff, int buffLen);
int fileDivide(FILE* fp);
int readFileBlock(FILE* fp, char* buff);
suseconds_t generate_timeout(struct timeval *sample_RTT, suseconds_t *estimated_RTT, suseconds_t *dev, int timeout_user);


#endif // UTILS_H_
