#ifndef SERVERFUNC_H_
#define SERVERFUNC_H_

#include "../INCLUDE/utils.h"
#include <ctype.h>


int gestisci_messaggio(int child_n, char *msg, int size, struct sockaddr* client_addr, socklen_t addrLen, int window_size, int p_loss, int timeout_user);

int handle_list(int child_sd, struct sockaddr* client_addr, socklen_t addrLen, int p_loss);

int handle_put(int child_sd, char *msg, int size, struct sockaddr* client_addr, socklen_t addrLen, int window_size, int p_loss, int timeout_user);

int handle_get(int child_sd, char *msg, int size, struct sockaddr* client_addr, socklen_t addrLen, int window_size, int p_loss, int timeout_user);

bool isNumber(char number[]);

#endif // SERVERFUNC_H_