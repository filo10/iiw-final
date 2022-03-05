#ifndef CLIENTFUNC_H_
#define CLIENTFUNC_H_

#include "../INCLUDE/utils.h"

int put(int sockfd, char *filename, unsigned long len, struct sockaddr_in servAddr, int servAddrLen, unsigned int seqN);

int get(int sockfd, char *filename, unsigned long len, struct sockaddr_in servAddr, int servAddrLen);

int list(int sockfd, char *msg, struct sockaddr_in servAddr, int servAddrLen);

#endif // CLIENTFUNC_H_