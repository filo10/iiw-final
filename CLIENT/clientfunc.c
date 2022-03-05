#include "clientfunc.h"



int put(int sockfd, char *filename, unsigned long len, struct sockaddr_in servAddr, int servAddrLen, unsigned int seqN) {
	char *buff = malloc(MAXLINE + 4);	// buffer x invio file al server
	char ack[16];	// buffer x ricevere ack
	Packet pkt;
	Queue queue;
	int i, n, nPktToAck, window_size, p_loss, timeout_user, seqnum, nPkt;

	// controlla se il file esiste; il nome del file lo trovo a partire dall'8° byte di "filename"
	FILE* fp;
	char *path;
	
	path = malloc(6+strlen(filename+7));
	sprintf(path, "FILES/");
	strcat(path, filename+7);
	
	fp = fopen(path, "rb");
	if (fp == NULL){
		perror("Errore in fopen()");
		exit(-1);
	}
	
	// imposta un timer di 5 sec in ascolto per gestire i casi di "disconnessione"
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt failed\n");
	
	
	// manda il comando al server e avvio il timer
	n = 0;
	fd_set set;
	struct timeval timeout2;
	while(n == 0){
			timeout2.tv_sec = 1;  // timer di 1 secondo per ritrasmettere il comando
			timeout2.tv_usec = 0;
			FD_ZERO(&set);
			FD_SET(sockfd, &set);
			sim_sendto(sockfd, filename, len, (struct sockaddr *)&servAddr, servAddrLen, LOSS_PROB);
			printf("Ho inviato %s\n", filename+4);
			n = select(sockfd + 1, &set, NULL, NULL, &timeout2); // se si verifica il timeout n = 0
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		else if (n > 0){ // n > 0: c'è qualcosa da leggere sul socket
			// riceve un ACK dal server, il quale lo sta mandando da una porta diversa
			if(recvfrom(sockfd, ack, 16, 0 , (struct sockaddr *)&servAddr , &servAddrLen) < 0){	// salva il nuovo indirizzo del server
				perror("Errore in ricezione ACK");
				exit(1);
			}
			if (memcmp(ack, &seqN, 4) == 0){
				printf("Ricevuto ACK corretto!\n");
				// Inizializzo i parametri di connessione
				memcpy(&window_size, ack + 4, 4);
				memcpy(&p_loss, ack + 8, 4);
				memcpy(&timeout_user, ack + 12, 4);
			}
			else {
				printf("Ricevuto un pacchetto con numero sequenza sbagliato.\n");
				printf("\n\tHo ricevuto dal server %d byte:\n\t%s\n", n, buff);
				return(-1);
			}
		}
		else printf("Ritrasmissione...\n");
	}
	
	// #### Inizio trasferimento file con selective repeat ####
	printf("Inizio invio file con S-R\n");
	queue = initQueue(window_size);
	nPkt = fileDivide(fp); // numero di pacchetti totali
	pkt.seq_num = seqN;
	int pktseq = seqN;
	nPktToAck = nPkt;
	pkt.data = (char*)malloc(sizeof(char)*MAXLINE);

	// Riempio inizialmente la finestra (coda circolare)
	i = 0;
	while(nPkt > 0 && (!isFull(&queue))){
		initializePacket(&pkt, pktseq, fp);
		enqueue(&queue, pkt);
		pktseq++;
		nPkt--;
		i++;
	}
	printf("Inseriti %d pacchetti in coda\n", i);
	printQueue(&queue, 1);

	// Inizializzo il timer e il RTT campione, stimato e deviazione
	suseconds_t timer;
	struct timeval sample_RTT;
	sample_RTT.tv_sec = 0;
	sample_RTT.tv_usec = 0;
	suseconds_t estimated_RTT = 3000;
	suseconds_t dev = 1500;
	timer = generate_timeout(&sample_RTT, &estimated_RTT, &dev, timeout_user);

	// Selective repeat
	struct timeval current_time;
	struct timeval elapsed_time;
	int baseSeq = seqN;
	int endSeq = seqN + window_size - 1;
	while (nPktToAck > 0){
		timeout2.tv_sec = 0;
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		while(1){
			n = select(sockfd + 1, &set, NULL, NULL, &timeout2);
			if (n < 0){
				perror("Errore in select");
				return -1;
			}
			// Se ho qualcosa da leggere sul socket lo leggo
			else if (n > 0){ 
				seqnum = getACKseqnum(sockfd, ack, sizeof(ack));
				printf("<< Ricevuto il riscontro del pacchetto con numero di sequenza %d >>\n", seqnum);

				// Gestisce il caso in cui il timer è troppo basso e il riscontro è già stato ricevuto
				int position;
				if (endSeq > baseSeq){
					if ((seqnum >= baseSeq) && (seqnum <= endSeq))
						position = (seqnum - baseSeq + queue.head) % queue.size;
					else{
						printf("Riscontro del pacchetto numero %d già ricevuto!\n", seqnum);
						continue;
					}
				}
				else{
					if (((seqnum > endSeq) && (seqnum < baseSeq)) == 0){ 
						if (seqnum >= baseSeq) position = (seqnum - baseSeq + queue.head) % queue.size;
						else position = ((seqN + 2 * window_size - baseSeq + seqnum - seqN + 1) + queue.head) % queue.size;
					}
					else{
						printf("Riscontro del pacchetto numero %d già ricevuto!\n", seqnum);
						continue;
					}
				}
				if (queue.pkt[position].pkt_state == 1){
					printf("Riscontro del pacchetto numero %d già ricevuto!\n", seqnum);
					continue;
				}

				// Prendo il tempo di ricezione
				if (timeout_user == 0){
					if (gettimeofday(&(current_time), NULL) == -1){
						perror("Errore: gettimeofday");
						exit(1);
					}
					printf("Tempo di ricezione: %ld\n", current_time.tv_usec);
				}

				// Aggiorno lo stato del pacchetto e stimo il nuovo timer se si è scelto adattativo
				if (queue.pkt[position].pkt_state != -1 && timeout_user == 0){
					printf("Tempo di invio: %ld\n", queue.pkt[position].send_time.tv_usec);
					timersub(&(current_time), &(queue.pkt[position].send_time), &sample_RTT);
					timer = generate_timeout(&sample_RTT, &estimated_RTT, &dev, timeout_user);
				}
				queue.pkt[position].pkt_state = 1;
				nPktToAck--;
				printQueue(&queue, 1);
			}
			// Se non ho più nulla da leggere allora invio
			else if (n == 0) break;
		}

		// Spostamento della finestra
		i = 0;
		while(queue.pkt[queue.head].pkt_state == 1){
			i++;
			dequeue(&queue);
			if(baseSeq < seqN + window_size * 2) baseSeq++;
			else baseSeq = seqN;
			if(endSeq < seqN + window_size * 2) endSeq++;
			else endSeq = seqN;
			if (nPkt > 0){
				initializePacket(&pkt, pktseq, fp);
				enqueue(&queue, pkt);				
				if (pktseq == seqN + 2 * window_size) pktseq = seqN;
				else pktseq++;
				nPkt--;
			}
		}
		if (i != 0){
			printf("Finestra spostata di %d posizioni\n", i);
			printQueue(&queue, 1);
		}

		// Spedizione pacchetti

		// Prima controllo i timeout
		if (gettimeofday(&(current_time), NULL) == -1){
			perror("Errore: gettimeofday");
			exit(1);
		}
		i = queue.head;
		int check = 0;
		while(1){
			if (queue.pkt[i].pkt_state != 1 && queue.pkt[i].pkt_state != 0){
				timersub(&current_time, &(queue.pkt[i].send_time), &elapsed_time);
				long int elap_time = elapsed_time.tv_usec + 1e6 * elapsed_time.tv_sec;
				if (elap_time > queue.pkt[i].timer){
					printf("\n!!! Pacchetto con numero di sequenza %d TIMEOUT !!!\n", queue.pkt[i].seq_num);
					queue.pkt[i].pkt_state = -1;
					queue.pkt[i].timer = queue.pkt[i].timer << 1;
					// Ritrasmissione
					memcpy(buff, &(queue.pkt[i].seq_num), 4);
					memcpy(buff + 4, queue.pkt[i].data, queue.pkt[i].dataSize);
					printf("\n\nPacchetto seqnum = %d in timeout rinviato con timer di %ld microsec\n\n", queue.pkt[i].seq_num, queue.pkt[i].timer);
					if (gettimeofday(&(queue.pkt[i].send_time), NULL) == -1){
						perror("Errore: gettimeofday");
						exit(1);
					}
					sim_sendto(sockfd, buff, queue.pkt[i].dataSize + 4, (struct sockaddr *) &servAddr, servAddrLen, p_loss);
					check = 1;
					break;
				}
			}
			if (i == queue.cursor) break;
			i = (i + 1) % queue.size;
		}
		if (check == 1) continue;

		// Trasmissione pacchetti

		if (!isEmpty(&queue)){
			if (queue.pkt[queue.cursor].pkt_state == 0 && queue.pkt[queue.cursor].seq_num != -1){
				queue.pkt[queue.cursor].timer = timer; // Imposto il timer
				memcpy(buff, &(queue.pkt[queue.cursor].seq_num), 4);
				memcpy(buff + 4, queue.pkt[queue.cursor].data, queue.pkt[queue.cursor].dataSize);
				printf("\n\nPacchetto seqnum = %d inviato con timer di %ld microsec\n\n", queue.pkt[queue.cursor].seq_num, queue.pkt[queue.cursor].timer);
				queue.pkt[queue.cursor].pkt_state = 2; // Pacchetto in trasmissione
				printf("##### Timer avviato per il pacchetto %d #####\n", queue.pkt[queue.cursor].seq_num);

				// Prendo il tempo di invio
				if (gettimeofday(&(queue.pkt[queue.cursor].send_time), NULL) == -1){
					perror("Errore: gettimeofday");
					exit(1);
				}
				// Invio
				sim_sendto(sockfd, buff, queue.pkt[queue.cursor].dataSize + 4, (struct sockaddr *) &servAddr, servAddrLen, p_loss);
				queue.cursor = (queue.cursor + 1) % queue.size;
			}
		}
	}
	clear(&queue);
	free(pkt.data);
	printf("\nFine Selective repeat\n\n");
	
	// Invio il messaggio di fine file
	printf("Trasmetto il messaggio ENDOFFILE\n");
	memcpy(buff, "ENDOFFILE", 9);
	sim_sendto(sockfd, buff, 9, (struct sockaddr *)&servAddr, servAddrLen, p_loss);
	while(1){
		timeout2.tv_sec = 1;  // timer di 1 secondo per ritrasmettere il comando
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		n = select(sockfd + 1, &set, NULL, NULL, &timeout2); // se si verifica il timeout n = 0
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		if (n > 0){ // n > 0: c'è qualcosa da leggere sul socket
			if (recvfrom(sockfd, ack, 3, 0, (struct sockaddr *)&servAddr , &servAddrLen) < 0) {
				perror("errore in recvfrom");
				exit(1);
			}
			if (strncmp(ack, "ACK", 3) == 0){
				printf("Ricevuto ACK di fine file!\n");
				break;
			}
			else {
				printf("Non ho ricevuto l'ack atteso per la fine del file.\n");
			}
		}
		if (n == 0) sim_sendto(sockfd, buff, 9, (struct sockaddr *)&servAddr, servAddrLen, p_loss);
	}
	if(fclose(fp)){
		perror("ERRORE in fclose\n");
		exit(1);
	};
	return 1;
}


int list(int sockfd, char *msg, struct sockaddr_in servAddr, int servAddrLen) {
	char list[MAXLINE];
	int i, n;
	
	// imposta un timer di 5 sec in ascolto per gestire i casi di "disconnessione"
	struct timeval timeout;      
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt failed\n");
	
	// manda il comando al server e avvio il timer
	fd_set set;
	struct timeval timeout2;
	while(1){
		timeout2.tv_sec = 1;  // timer di 1 secondo per ritrasmettere il comando
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		sim_sendto(sockfd, msg, 6, (struct sockaddr *)&servAddr, servAddrLen, LOSS_PROB);
		n = select(sockfd + 1, &set, NULL, NULL, &timeout2);
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		// Se ho qualcosa da leggere sul socket lo leggo
		else if (n > 0){
			int n = recvfrom(sockfd, list, sizeof(list), 0 , NULL, NULL); // La risposta arriva dal server su un altro numero di porta
			if (n < 0) {
				perror("Errore in ricezione list dal server");
				exit(1);
			}
			if(n > 0) {
				printf("\nFile presenti sul server:\n\n%s", list);
				break;
			}
		}
	}
	return 1;
}


int get(int sockfd, char *filename, unsigned long len, struct sockaddr_in servAddr, int servAddrLen) {
	char ack[4];
	char *recv_buff = malloc(MAXLINE + 4);
	FILE *fp;
	Packet pkt;
	Queue queue;
	int i, n, start_seqN, nBytes, window_size, p_loss, timeout_user;
	pkt.data = (char*)malloc(MAXLINE);

	// mi ricordo il seqN iniziale
	memcpy(&start_seqN, filename, 4);


	// imposta un timer di 5 sec in ascolto per gestire i casi di "disconnessione"
	struct timeval timeout;      
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt failed\n");

	// mando il comando al server e avvio il timer
	fd_set set;
	struct timeval timeout2;
	while(1){
		timeout2.tv_sec = 1;  // timer di 1 secondo per ritrasmettere il comando
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		sim_sendto(sockfd, filename, len, (struct sockaddr *)&servAddr, servAddrLen, LOSS_PROB);
		n = select(sockfd + 1, &set, NULL, NULL, &timeout2);
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		// Se ho qualcosa da leggere sul socket lo leggo
		else if (n > 0){
			// aspetto un ack da un socket diverso
			n = recvfrom(sockfd, recv_buff, MAXLINE, 0 , (struct sockaddr *)&servAddr , &servAddrLen);	// salva il nuovo indirizzo del server
			if (n < 0) {
				perror("Errore in ricezione ACK");
				exit(1);
			}
			if (memcmp(recv_buff, &start_seqN, 4) == 0){
				printf("Ricevuto ACK corretto!\n");
				// Inizializzo i parametri di connessione
				memcpy(&window_size, recv_buff + 4, 4);
				memcpy(&p_loss, recv_buff + 8, 4);
				memcpy(&timeout_user, recv_buff + 12, 4);
				break;
			}
			else {
				printf("Ricevuto un pacchetto con numero sequenza sbagliato.\n");
				return(-1);
			}
		}
	}
	
	// manda ack ho ricevuto nuova porta
	sim_sendto(sockfd, recv_buff, 4, (struct sockaddr *)&servAddr, servAddrLen, p_loss);

	// apro (o creo) il file; il nome del file lo trovo a partire dall'8° byte di "filename"
	char *path;
	
	path = malloc(6+strlen(filename+7));
	sprintf(path, "FILES/");
	strcat(path, filename+7);
	
	fp = fopen(path, "wb+");
	if (fp == NULL){
		perror("Errore in fopen()");
		exit(-1);
	}

	// Ricezione selective repeat
	int baseSeq = start_seqN;
	int endSeq = start_seqN + window_size - 1; // range di numeri di sequenza di pacchetti accettabili in finestra
	queue = initQueue(window_size);

	while(1) {
		// ricevo un pacchetto (seqN|data)
		if ((nBytes = recvfrom(sockfd, recv_buff, MAXLINE + 4, 0, (struct sockaddr *)&servAddr, &servAddrLen)) < 0) {
			perror("errore in recvfrom");
			//exit(1);
			return -1;
		}
		printf("\n\t##########################\n\t# Bytes ricevuti dal server: %d\n\t##########################\n\n", nBytes);
		if (strncmp(recv_buff, "ENDOFFILE", 9) == 0) break;  // se ricevo ENDOFFILE esco da while

		// mando ack
		memcpy(ack, recv_buff, sizeof(int));
		sim_sendto(sockfd, ack, sizeof(ack), (struct sockaddr *)&servAddr, servAddrLen, p_loss);

		// metto in finestra di ricezione
		memcpy(&(pkt.seq_num), ack, sizeof(int));
		printf("<< Mandato ack del pacchetto con numero di sequenza %d >>\n", pkt.seq_num);
		memcpy(pkt.data, recv_buff + sizeof(int), nBytes - 4);
		pkt.dataSize = nBytes - 4;
		int position;
		if (endSeq > baseSeq){
			if ((pkt.seq_num >= baseSeq) && (pkt.seq_num <= endSeq))
				position = pkt.seq_num - baseSeq;
			else continue;
		}
		else{
			if (!((pkt.seq_num > endSeq) && (pkt.seq_num < baseSeq))){ 
				if (pkt.seq_num >= baseSeq) position = pkt.seq_num - baseSeq;
				else position = start_seqN + 2 * window_size - baseSeq + pkt.seq_num - start_seqN + 1;
			}
			else continue;
		}
		if (queue.pkt[(position + queue.head) % queue.size].seq_num != pkt.seq_num) insert(&queue, pkt, position);
		printf("Pacchetto numero %d inserito in finestra in posizione %ld\n", pkt.seq_num, (position + queue.head) % queue.size);

		// sposto finestra di ricezione se in ordine e scrivo su file
		i = queue.head;
		while(queue.pkt[i].seq_num != -1){
			printf("Scrivo su file i dati del pacchetto %d\n", queue.pkt[i].seq_num);
			fwrite(queue.pkt[i].data, 1, queue.pkt[i].dataSize, fp);
			queue.pkt[i].seq_num = -1;
			dequeue(&queue);
			if(baseSeq < start_seqN + window_size * 2) baseSeq++;
			else baseSeq = start_seqN;
			if(endSeq < start_seqN + window_size * 2) endSeq++;
			else endSeq = start_seqN;
			i = (i + 1) % queue.size;
		}
		printf("\nFINESTRA DOPO LO SPOSTAMENTO\n");
		printQueue(&queue, 0); 	
	}
	clear(&queue);

	// Invio ack per fine file
	fprintf(stdout,"Invio ACK per fine file\n");
	memcpy(ack, "ACK", 3);
	n = 1;
	while(n){
		timeout2.tv_sec = 2;  // aspetta 2 secondi per capire se il client ha ricevuto ack di fine file
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		sim_sendto(sockfd, ack, 3, (struct sockaddr *)&servAddr, servAddrLen, p_loss);
		n = select(sockfd + 1, &set, NULL, NULL, &timeout2); // se si verifica il timeout n = 0
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		if (n > 0){ // n > 0: c'è qualcosa da leggere sul socket
			if (recvfrom(sockfd, recv_buff, 9, 0, (struct sockaddr *)&servAddr , &servAddrLen) < 0) {
				perror("errore in recvfrom");
				exit(1);
			}
		}
	}
	if(fclose(fp)){
		perror("ERRORE in fclose\n");
		exit(1);
	}
	
	printf("\tfine gest_msg(), comando GET, ritorno\n");
	return 1;
}


