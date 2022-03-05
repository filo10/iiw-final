#include "serverfunc.h"



int gestisci_messaggio(int child_n, char *msg, int size, struct sockaddr* client_addr, socklen_t addrLen, int window_size, int p_loss, int timeout_user) {
	int child_sd;
	char ack[5];
	struct sockaddr_in child_addr;
	
	printf("\n\t##########################\n\t# Bytes ricevuti dal client: %d\n\t# Messaggio ricevuto dal client:\n\t#\n\t# %s\n\t#\n\t##########################\n\n", size, msg+4);
	
	// nuovo socket, bind
	if ((child_sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Errore in socket");
		exit(1);
	}
	
	// inizializzo indirizzo
	memset((void *)&child_addr, 0, sizeof(child_addr));
	child_addr.sin_family = AF_INET;
	child_addr.sin_addr.s_addr = htonl(INADDR_ANY); // il server accetta pacchetti su una qualunque delle sue interfacce di rete
	child_addr.sin_port = htons(SERV_PORT + child_n); // numero di porta del server
	
	// assegno l'indirizzo al socket
	bind_socket(child_sd, child_addr);
	
	printf("\t@FIGLIO %d: Socket inizializzato\n", child_n);
	
	// gestione caso LIST (il comando lo leggo dal 5° byte di msg)
	if(strncmp(msg+4, "ls", 2) == 0)
		handle_list(child_sd, client_addr, addrLen, p_loss);
	
	// gestione caso PUT
	if(strncmp(msg+4, "put", 3) == 0) {
		handle_put(child_sd, msg, size, client_addr, addrLen, window_size, p_loss, timeout_user);
	}
	
	// gestione caso GET
	if(strncmp(msg+4, "get", 3) == 0) {
		handle_get(child_sd, msg, size, client_addr, addrLen, window_size, p_loss, timeout_user);
	}
	
	printf("\tfine gestisci_messaggio() ritorno\n");
	return 1;
}


int handle_list(int child_sd, struct sockaddr* client_addr, socklen_t addrLen, int p_loss) {
	char list[MAXLINE];
	int len;
	
	memset((void *)&list, 0, sizeof(list));
	
	printf("# entrato in handle list\n");
	
	//stampa ls in una stringa
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir ("FILES")) != NULL) {
		// print all the files and directories within directory
		while ((ent = readdir (dir)) != NULL) {
			if(strncmp(ent->d_name, ".", 1) == 0)
				continue;
			strcat(list, " - ");
			strcat(list, ent->d_name);
			strcat(list, "\n");
		}
		closedir (dir);
	} else {
		// could not open directory
		perror ("");
		return EXIT_FAILURE;
	}
	len = sizeof(list);
	sim_sendto(child_sd, list, len, client_addr, addrLen, p_loss);
	printf("\tfine gest_msg(), comando LIST, ritorno\n");
	return 1;
}


int handle_put(int child_sd, char *msg, int size, struct sockaddr* client_addr, socklen_t addrLen, int window_size, int p_loss, int timeout_user) {
	char ack[16];
	char *recv_buff = malloc(MAXLINE + 4);
	FILE *fp;
	Packet pkt;
	Queue queue;
	int i, n, nBytes, start_seqN;
	pkt.data = (char*)malloc(MAXLINE);
	queue = initQueue(window_size);
	printf("# entrato in handle put\n");

	
	// controllo che il file esiste, e lo apro; nel caso gia esiste sovrascrivo
	char *path;
	path = malloc(6+strlen(msg+7));
	// metti in path il nome della directory
	sprintf(path, "FILES/");
	// aggiungi a path il nome del file; ora path è il percorso del file
	strcat(path, msg+7);
	
	fp = fopen(path, "w+");
    if (fp == NULL){
		fprintf(stderr, "File non aperto");
		exit(1);
    }
	
	// imposta un timer di 5 sec in ascolto per gestire i casi di "disconnessione"
	struct timeval timeout;      
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt (child_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        perror("setsockopt failed\n");
	
	// mando ack da questo nuovo socket (ack = seq# ricevuto) e i parametri di connessione
	memcpy(ack, msg, 4);
	memcpy(ack + 4, &window_size, 4);
	memcpy(ack + 8, &p_loss, 4);
	memcpy(ack + 12, &timeout_user, 4);

	sim_sendto(child_sd, ack, sizeof(ack), client_addr, addrLen, p_loss);
	
	// mi ricordo il seqN iniziale
	memcpy(&start_seqN, msg, 4);
	
	// Ricezione selective repeat
	int baseSeq = start_seqN;
	int endSeq = start_seqN + window_size - 1; // range di numeri di sequenza di pacchetti accettabili in finestra
	while(1) {
		// ricevo un pacchetto (seqN|data)
		if ((nBytes = recvfrom(child_sd, recv_buff, MAXLINE + 4, 0, (struct sockaddr *)&client_addr, &addrLen)) < 0) {
			perror("errore in recvfrom");
			//exit(1);
			return -1;
		}
		printf("\n\t##########################\n\t# Bytes ricevuti dal client: %d\n\t##########################\n\n", nBytes);
		if (strncmp(recv_buff, "ENDOFFILE", 9) == 0) break;  // se ricevo ENDOFFILE esco da while e chiudo file

		// mando ack
		memcpy(ack, recv_buff, sizeof(int));
		sim_sendto(child_sd, ack, sizeof(ack), (struct sockaddr *)&client_addr, addrLen, p_loss);
      	
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
			if(baseSeq < start_seqN+ window_size * 2) baseSeq++;
			else baseSeq = start_seqN;
			if(endSeq < start_seqN + window_size * 2) endSeq++;
			else endSeq = start_seqN;
			i = (i + 1) % queue.size;
        }
		printf("\nFINESTRA DOPO LO SPOSTAMENTO\n");
		printQueue(&queue, 0); 	
    }
	clear(&queue);

	// Invio ack di fine file
    fprintf(stdout,"Invio ACK per fine file\n");
	memcpy(ack, "ACK", 3);
	n = 1;
	fd_set set;
	struct timeval timeoutEOF;
	while(n){
		timeoutEOF.tv_sec = 2;  // aspetta 2 secondi per capire se il client ha ricevuto ack di fine file
		timeoutEOF.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(child_sd, &set);
    	sim_sendto(child_sd, ack, 3, (struct sockaddr *)&client_addr, addrLen, p_loss);
		n = select(child_sd + 1, &set, NULL, NULL, &timeoutEOF); // se si verifica il timeout n = 0
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		if (n > 0){ // n > 0: c'è qualcosa da leggere sul socket
			if (recvfrom(child_sd, recv_buff, 3, 0, (struct sockaddr *)&client_addr, &addrLen) < 0) {
				perror("errore in recvfrom");
				return -1;
				//exit(1);
			}
		}
	}
	if(fclose(fp)){
		perror("ERRORE in fclose\n");
		exit(1);
	};
	printf("\tfine gest_msg(), comando PUT, ritorno\n");
	return 1;
}


int handle_get(int child_sd, char *msg, int size, struct sockaddr* client_addr, socklen_t addrLen, int window_size, int p_loss, int timeout_user) {
	char *buff = malloc(MAXLINE + 4);	// buffer x invio file al server
	char ack[sizeof(int)*4];	// buffer x ricevere messaggi dal server
	Packet pkt;
	Queue queue;
	int i, nPktToAck, nPkt, n, seqnum;

	printf("# entrato in handle get\n");
	
	// imposta un timer di 5 sec in ascolto per gestire i casi di "disconnessione"
	struct timeval timeout;      
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt (child_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
        perror("setsockopt failed\n");
	}

	// controllo che il file esiste, e lo apro; nel caso gia esiste sovrascrivo
	FILE* fp;
	char *path;
	path = malloc(6+strlen(msg+7));
	// metti in path il nome della directory
	sprintf(path, "FILES/");
	// aggiungi a path il nome del file; ora path è il percorso del file
	strcat(path, msg+7);
	fp = fopen(path, "rb+");
    	if (fp == NULL){
		fprintf(stderr, "File non aperto");
		exit(1);
    	}
	
	// mando ack da questo nuovo socket (ack = seq# ricevuto)
	memcpy(ack, msg, 4);
	memcpy(ack + 4, &window_size, 4);
	memcpy(ack + 8, &p_loss, 4);
	memcpy(ack + 12, &timeout_user, 4);
	int seqN;
	sim_sendto(child_sd, ack, sizeof(ack), client_addr, addrLen, p_loss);
	memcpy(&seqN, ack, 4);

	// Aspetto ack dal client per cominciare a inviare il file
	n = recvfrom(child_sd, ack, 4, 0 , (struct sockaddr *)&client_addr , &addrLen);	// salva il nuovo indirizzo del server
	if (n < 0) {
		perror("Errore in ricezione ACK");
		return -1;
		//exit(1);
	}
	if (memcmp(ack, &seqN, 4) == 0){
		printf("Ricevuto ACK corretto!\n");
	}
	else {
		printf("Ricevuto un pacchetto con numero sequenza sbagliato.\n");
		return(-1);
	}
	
	// Invio il file, usando S-R
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
	sample_RTT.tv_usec = 0;
	sample_RTT.tv_sec = 0;
	suseconds_t estimated_RTT = 3000;
	suseconds_t dev = 1500;
	timer = generate_timeout(&sample_RTT, &estimated_RTT, &dev, timeout_user);

	// Selective repeat
	struct timeval current_time;
	struct timeval elapsed_time;
	fd_set set;
	struct timeval timeout2;
	int baseSeq = seqN;
	int endSeq = seqN + window_size - 1;
	while (nPktToAck > 0){
		timeout2.tv_sec = 0;
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(child_sd, &set);
		while(1){
			n = select(child_sd + 1, &set, NULL, NULL, &timeout2);
			if (n < 0){
				perror("Errore in select");
				return -1;
			}
			// Se ho qualcosa da leggere sul socket lo leggo
			else if (n > 0){ 
				seqnum = getACKseqnum(child_sd, ack, sizeof(ack));
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
					sim_sendto(child_sd, buff, queue.pkt[i].dataSize + 4, (struct sockaddr *) &client_addr, addrLen, p_loss);
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
				sim_sendto(child_sd, buff, queue.pkt[queue.cursor].dataSize + 4, (struct sockaddr *) &client_addr, addrLen, p_loss);
				queue.cursor = (queue.cursor + 1) % queue.size;
			}
    	}
  	}
	clear(&queue);
	free(pkt.data);
	printf("\nFine Selective repeat\n\n");
	
	
	// Messaggio di fine file
	printf("Trasmetto il messaggio ENDOFFILE\n");
	memcpy(buff, "ENDOFFILE", 9);
	while(1){
		timeout2.tv_sec = 2;  // aspetta 2 secondi per capire se il client ha ricevuto ack di fine file
		timeout2.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(child_sd, &set);
    	sim_sendto(child_sd, buff, 9, (struct sockaddr *)&client_addr, addrLen, p_loss);
		n = select(child_sd + 1, &set, NULL, NULL, &timeout2); // se si verifica il timeout n = 0
		if (n < 0){
			perror("Errore in select");
			return -1;
		}
		if (n > 0){ // n > 0: c'è qualcosa da leggere sul socket
			// TODO tolto int a riga sotto
			n = recvfrom(child_sd, ack, 3, 0 , NULL, NULL);
			if (n < 0) {
				perror("Errore in ricezione ack di fine file dal client");
				return -1;
				//exit(1);
			}
			if (n > 0) {
				if (strncmp(ack, "ACK", 3) == 0){
				printf("\nACK di fine file ricevuto\n");
				break;
			}
				else{
					printf("Ho ricevuto un ack non conforme all'endoffile.\n");
				}
			}
		}
	}
	// Chiudo il file
    	if(fclose(fp)){
		perror("ERRORE in fclose\n");
		exit(1);
	}
	
	printf("\tfine gest_msg(), comando GET, ritorno\n");
	return 1;
}


bool isNumber(char number[]) {
    int i = 0;

    //checking for negative numbers
    if (number[0] == '-')
    	return false;
        //i = 1;
    for (; number[i] != 0; i++) {
        //if (number[i] > '9' || number[i] < '0')
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}


