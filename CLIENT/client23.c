#include "../INCLUDE/utils.h"
#include "clientfunc.h"



int main(int argc, char **argv) {
	
	int sockfd, servAddrLen;
	unsigned int seqN;
	char command[MAXLINE-4];
	char input[MAXLINE-4];
	char msg[MAXLINE];
	int pid;
	struct sockaddr_in servAddr;
	
	// creo il socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Errore in socket");
		exit(1);
	}
	// init address
	memset((void *)&servAddr, 0, sizeof(servAddr));	// azzera servaddr
	servAddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, "127.0.0.1", &servAddr.sin_addr) <= 0) {	// inet_pton (p=presentation) vale anche per indirizzi IPv6
		fprintf(stderr, "errore in inet_pton per %s", argv[1]);
		exit(1);
	}
	servAddr.sin_port = htons(5193); // numero di porta del server
	
	servAddrLen = sizeof(servAddr);
	
	printf("Client avviato. Digita 'h' per aiuto\n");
	
	// chiedi all'utente cosa vuole fare
	printf("Cosa vuoi fare?\t>>> ");
	
	// gestisci il comando:
	while(1) {
		
		// elimina dalla memoria il pkt preparato in precedenza
		memset((void *)msg, 0, sizeof(msg));
		memset((void *)input, 0, sizeof(input));
		memset((void *)command, 0, sizeof(command));
		
		// crea un unsigned int casuale da usare come sequence number
		srand(time(NULL));
		seqN = (unsigned int) rand()*rand() % 1000; //(2^15 - 1)x(2^15 -1)	dovrebbe sempre essere == 4 byte
		
		// scrivilo nei primi 4 byte della stringa msg (bitwise)
		memcpy(msg, &seqN, 4);
		
		// scrivi in coda a command il comando, chiedendolo all'utente	// andrebbe limitato l'input... typing 1019+ chars -> infinite loop!!!
		if(scanf("%[^\n]%*c", input) == EOF){
			printf("Errore in scanf\n");
		}	// %*c fa in modo che scanf elimini ultimo char in input (i.e. '\n') ...
		// ... se non ci fosse, '\n' rimarrebbe sempre in command e scanf() ritornerebbe immediatamente, girando il while infinitamente.
		
		char *tmp;
		tmp = strtok(input, " ");
		strcat(command, tmp);
		while(1) {
			tmp = strtok(NULL, " ");
			if(tmp == NULL)
				break;
			strcat(command, tmp);
		}
		
		// "appendi" il comando nel messaggio
		snprintf(msg+4, MAXLINE+1, "%s", command);
		msg[MAXLINE+4] = 0;

		pid = fork();
		if (pid == 0) {
			if(strncmp(command, "put", 3) == 0){
				put(sockfd, msg, sizeof(msg), servAddr, servAddrLen, seqN);
			}
			else if(strncmp(command, "get", 3) == 0){
				get(sockfd, msg, sizeof(msg), servAddr, servAddrLen);
			}
			else if(strncmp(command, "ls", 2) == 0){
				list(sockfd, msg, servAddr, servAddrLen);
			}
			printf("\n>>> ");
			exit(0);
		}
		if(strncmp(command, "put", 3) == 0){
		}
		else if(strncmp(command, "get", 3) == 0){
		}
		else if(strncmp(command, "ls", 2) == 0){
		}
		else if(strncmp(command, "q", 1) == 0){
			printf("\nA presto!\n");
			break;
		}
		else if(strncmp(command, "h", 1) == 0){
			printf("\nPuoi digitare i seguenti comandi:\n\n - ls\t\t: ricevi l'elenco dei file presenti sul server\n - put <file>\t: carica sul server 'file'\n - get <file>\t: scarica dal server 'file'\n - q\t\t: esci\n");
		}
		else if(strncmp(command, "davide", 6) == 0){
			printf("Modalità ULTRAPRESTAZIONE avviata!!!!! Cancello indentazione nei files sorgente!\n");
		}
		else {
			printf("Comando non valido. Digita 'h' per aiuto\n");
		}
		printf("\n");
	}
	
	return 0;
}


