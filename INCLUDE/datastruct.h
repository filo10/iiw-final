#ifndef DATASTRUCT_H_
#define DATASTRUCT_H_

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>
#include <semaphore.h>
#include "utils.h"


#define MAXLINE		1024

// p, t, N
#define LOSS_PROB	10
#define TIMEOUT		1000
#define WINDSIZE	23


typedef struct packet{
	int seq_num;
	char *data;
	int dataSize;
	int pkt_state; // | -1: in timeout | 0: non inviato | 1: Ricevuto | 2: in trasmissione |
	struct timeval send_time; //tempo alla prima trasmissione
	suseconds_t timer; //timer del pacchetto
}Packet;


typedef struct queue{
	unsigned int size;
	long int head;
	long int tail;
	long int cursor;
	Packet *pkt;
}Queue;


// Operazioni pacchetto
void initializePacket(Packet* pkt, int seqN, FILE *fp);

// Operazioni coda
Queue initQueue(unsigned int);
void printQueue(Queue* queue, int printState);
int insert(Queue *queue, Packet pkt, int position);
int enqueue(Queue *queue, Packet pkt);
int dequeue(Queue *queue);
int isEmpty(Queue *queue);
int isFull(Queue *queue);
int clear(Queue *queue);


#endif // DATASTRUCT_H_
