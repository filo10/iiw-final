#include "datastruct.h"
#include "utils.h"


void initializePacket(Packet *pkt, int seqN, FILE* fp){
	pkt->seq_num = seqN;
	pkt->pkt_state = 0;
	pkt->dataSize = readFileBlock(fp, pkt->data);
}


Queue initQueue(unsigned int n){
	Queue tmp = {0, -1, -1, -1, NULL};
	tmp.pkt = malloc(n * sizeof(Packet));
	if(tmp.pkt == NULL){
		perror("Errore in malloc queue");
		exit(-1);
	}
	tmp.size = n;
	int i;
	for(i = 0; i < n ; i++){
		tmp.pkt[i].data = malloc(MAXLINE);
		tmp.pkt[i].seq_num = -1;
	}
	return tmp;
}


int isEmpty(Queue *queue){
	return queue == NULL || queue->head == -1;
}


int isFull(Queue *queue){
	return queue == NULL || queue->pkt == NULL || ((queue->tail + 1) % queue->size == queue->head);
}


void printQueue(Queue *queue, int printState){
	if(isEmpty(queue)){
		printf("Coda vuota\n");
		return;
	}
	printf("\n\t<=\t");
	int i = queue->head;
	do{
		printf("| %d |", queue->pkt[i].seq_num);
		i = (i + 1) % queue->size;
	}while(i != (queue->tail + 1) % queue->size);
	printf("\t<=\n");
	printf("\n\t\t");
	if (printState == 0)
		return;
	i = queue->head;
	do{
		printf("| %d |", queue->pkt[i].pkt_state);
		i = (i + 1) % queue->size;
	}while(i != (queue->tail + 1) % queue->size);
	printf("\n\n");
}


// Inserisce un elemento in una posizione qualsiasi della coda
int insert(Queue *queue, Packet pkt, int position){
	if (queue == NULL){
		printf("Errore in insert: coda non inizializzata\n");
		return 1;
	}
	if (position < 0 || position >= queue->size){
		printf("Errore in insert: indice di posizione %d non valido!\n", position);
		return 1;
	}
	else{
		// Aggiornmento indici di testa e coda se primo inserimento
		if (isEmpty(queue)){
		queue->head = 0;
		queue->tail = 0;
		}
		// Inserisco il pacchetto
		int index = (position + queue->head) % queue->size;
		queue->pkt[index].seq_num = pkt.seq_num;
		queue->pkt[index].dataSize = pkt.dataSize;
		printf("Sto per inserire data\n");
		memcpy(queue->pkt[index].data, pkt.data, pkt.dataSize);
		// Aggiornamento dell'indice di coda
		if (queue->tail >= queue->head){
			if (index < queue->head)
				queue->tail = index;
			else {
				if(index > queue->tail)
					queue->tail = index;
			}
		}
		else{
			if (index > queue->tail && index < queue->head)
				queue->tail = index;
		}
	}
	return 0;
}


// Inserisce un elemento in coda e ritorna 0 in caso di successo, 1 altrimenti
int enqueue(Queue *queue, Packet pkt){
	if (isFull(queue)){
		printf("Impossibile inserire pacchetto nella finestra: coda piena\n");
		return 1;
	}
	else{
		if (isEmpty(queue)){
			queue->tail = 0;
			queue->head = 0;
			queue->cursor = 0;
		}
		else{
			queue->tail = (queue->tail + 1) % queue->size;
		}
		queue->pkt[queue->tail].pkt_state = pkt.pkt_state;
		queue->pkt[queue->tail].seq_num = pkt.seq_num;
		queue->pkt[queue->tail].dataSize = pkt.dataSize;
		memcpy(queue->pkt[queue->tail].data, pkt.data, pkt.dataSize);
		return 0;
	}
}

// Aumenta l'indice di testa (non elimina fisicamente l'elemento). Ritorna 0 in caso di successo, 1 altrimenti
int dequeue(Queue *queue){
	if (isEmpty(queue)){
		perror("Operazione dequeue fallita: coda vuota");
		return 1;
	}
	else{
		if (queue->head == queue->tail){
			queue->head = -1;
			queue->tail = -1;
		}
		else{
			queue->head = (queue->head + 1) % queue->size;
		}
		return 0;
	}
}

// Dealloca lo spazio di memoria allocato per la queue. Ritorna 0 in caso di successo
int clear(Queue *queue){
	if(queue == NULL || queue->size == 0){
		printf("Queue uninitialized\n");
		return 1;
	}
	int i;
	for (i = 0; i < queue->size; i++){
		free(queue->pkt[i].data);
	}
	free(queue->pkt);
	return 1;
}

