#include "utils.h"



int bind_socket(int sockfd, struct sockaddr_in addr) {
	// assegna l'indirizzo al socket
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Errore in bind");
		exit(1);
	}
}


int is_pkt_lost(int prob) {
	// calcola probabilita' di perdita
	int rand_n;
	
	//srand(time(NULL));
	rand_n = (rand()%100); //genera un numero da 0 a 99

	if (rand_n < prob)	// es prob=40, la prob che un numero a caso tra 0 e 99 è <40 = 40
		return (1);	// pkt loss
	else
		return (0);
}


int sim_sendto(int socketd, char *msg, unsigned long len, struct sockaddr* addr, socklen_t addr_len, int p_loss) {
	// invia un pkt simulando la possibilità di perderlo
	int ret;
	
	if (is_pkt_lost(p_loss) == 1) {
		printf("\n!!! PACCHETTO PERSO !!!\n");
		return len;
	}
	
	// sendto
	if (ret = sendto(socketd, msg, len, 0, addr, addr_len) < 0) {
		perror("errore in sendto");
		exit(1);
	}
	return ret;
}


int getACKseqnum(int sockfd, char* buff, int buffLen){
	int n = recvfrom(sockfd, buff, buffLen, 0 , NULL, NULL);
	if (n < 0) {
		perror("errore in getACKseqnum");
		exit(1);
	}
	int seqnum;
	memcpy(&seqnum, buff, 4);   
	return seqnum;
}


int fileDivide(FILE* fp){
	/* Get the size of the file */
	fseek(fp, 0, SEEK_END);
	int fsize = ftell(fp);
	printf("Size of file: %d\n", fsize);
	fseek(fp, 0, SEEK_SET);
	/* find the number of packets to divide the file into */
	int n = fsize/MAXLINE + ((fsize % MAXLINE) != 0);
	printf("File diviso in %d pacchetti\n", n);
	return n;
}


int readFileBlock(FILE* fp, char* buff){
	// ritorna il numero di bytes letti e ne salva in buff il contenuto
	int nbytes;
	nbytes = fread(buff, 1, MAXLINE, fp);
	return nbytes; 
}


suseconds_t generate_timeout(struct timeval *sample_RTT, suseconds_t *estimated_RTT, suseconds_t *dev, int timeout_user){
	double alpha = 0.125;
	double beta = 0.25;
	suseconds_t timeout;
	if(timeout_user == 0){
		if(sample_RTT == 0){
			printf("Stima iniziale RTT: %ld microsec\n", *estimated_RTT);
			printf("Stima iniziale dev: %ld microsec\n", *dev);
		}
		else{
			suseconds_t usecSampleRTT = sample_RTT->tv_usec + sample_RTT->tv_sec * 1e6;
			printf("SampleRTT vero: %ld microsec\n", usecSampleRTT);
			*estimated_RTT = (1-alpha) * (*estimated_RTT) + (alpha * (usecSampleRTT));
			*dev = (1-beta) * (*dev) + (beta * abs(usecSampleRTT - *estimated_RTT));
      	}
      	timeout = *estimated_RTT + (4 * (*dev));
	}
	else{
		timeout = timeout_user;
	}
	return timeout;
}


