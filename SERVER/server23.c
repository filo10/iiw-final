#include "../INCLUDE/utils.h"
#include "serverfunc.h"



key_t sem_key = 3333;
long id_semaforo;
struct sembuf aspetta, segnala;



int main(int argc, char **argv) {

	int sockfd, rcvd_bytes, child_counter = 0;
	struct sockaddr_in addr;
	socklen_t len;
	char buff[MAXLINE + 4];
	pid_t pid;

	
	if(argc != 1 && argc != 4){
		printf("Utilizzo: %s\toppure\t%s <dim. finestra> <prob. perdita> <timeout>\n", argv[0], argv[0]);
		return 0;
	}
	
	// parametri p, t, n di default
	int window_size = WINDSIZE;
	int timeout_user = TIMEOUT;
	int p_loss = LOSS_PROB;
	
	
	// controlla che i parametri in input sono numeri
	if(argc == 4) {
		bool ret = false;
		for(int j = 1; j < 4; j++) {
			if(!isNumber(argv[j])) {
				printf("Attenzione: argomento %d non è un numero positivo.\n", j);
				ret = true;
			}
		}
		if(ret)	return 0;
		window_size = strtol(argv[1], NULL, 10);
		p_loss = strtol(argv[2], NULL, 10);
		timeout_user = strtol(argv[3], NULL, 10);
	}	
	if(p_loss > 100) {
		printf("prob. perdita deve essere tra 0 e 100\n");
		return 0;
	}
	if(timeout_user < 10) {
		printf("\n!!!\tTimer scelto troppo basso. Ho impostato il timer di default: %d\n\n", TIMEOUT);
		timeout_user = TIMEOUT;
	}
	
	printf("Puoi impostare i parametri usando: %s <dim. finestra> <prob. perdita> <timeout>\n", argv[0]);
	
	printf("Impostando il parametro <timeout> a 0 si avvia il timer adattativo, altrimenti viene impostato il valore inserito (in microsecondi) per il timer fisso\n\n");
	
	printf("Server avviato, con parametri:\n\tDimensione Finestra: %d\n\tProbabilità di perdita: %d\n", window_size, p_loss);
	
	if(timeout_user == 0){
		printf("\tTimer: %d -> adattativo\n", timeout_user);
	}
	else {
		printf("\tTimer: %d microsec (fisso)\n", timeout_user);
	}
	printf("\nBenvenuto! PID: %d\n", getpid());
	
	// init semaforo limita processi-figlio:
	// il semaforo parte con un valore MAX_N_CHILD
	// ad ogni spawn di un figlio, decresce di 1 (operazione "aspetta");
	// quando un figlio termina, aumenta di 1 (operazione "segnala")
	
	// se il valore del semaforo diventa negativo con l'operazione "aspetta"
	// il processo padre che ha usato suddetta operazione entra in stato di wait
	// fino a quando il valore non torna >0, grazie ad una operazione segnala
	
	id_semaforo = semget(sem_key, 1, IPC_CREAT|0666);	// crea un semaforo ad un solo valore.
	semctl(id_semaforo, 0, SETVAL, MAX_N_CHILD);	// setta il valore del semaforo a 1 = max_n_xhild
	
	// init funz wait per semaforo
	aspetta.sem_num = 0;	// per il 0-simo valore del semaforo
	aspetta.sem_op = -1;	// fa-1, se il valore del semaforo diventa <0, si mette in attesa che ritorni >=0
	aspetta.sem_flg = 0;
	
	// init funz signal per semaforo
	segnala.sem_num = 0;
	segnala.sem_op = 1;	// valore sem +=1
	segnala.sem_flg = 0;

	// creo il socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Errore in socket");
		exit(1);
	}
	// inizializzo indirizzo
	memset((void *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // il server accetta pacchetti su una qualunque delle sue interfacce di rete
	addr.sin_port = htons(SERV_PORT); // numero di porta del server
	// assegno l'indirizzo al socket
	bind_socket(sockfd, addr);
	len = sizeof(addr);
	printf("Socket inizializzato\n");
	
	// mettiti in ascolto di messaggi e spawna un figlio per ogni messaggio affiche questo lo gestisca
	while(1) {		
		// SEMAFORO: aspetta in caso siano nati max_n figli
		semop(id_semaforo, &aspetta, 1);
		printf("@PADRE: sono in ascolto!\n\n");
		child_counter++;
		
		// mettiti in ricezione
		if ( ( rcvd_bytes = recvfrom(sockfd, buff, sizeof(buff), 0, (struct sockaddr *)&addr, &len) ) < 0 ) {
			perror("errore in recvfrom");
			exit(1);
		}
		
		// se arriva qualcosa
		if (rcvd_bytes > 0) {
			// crea un figlio
			pid = fork();
			if(pid < 0) {
				perror("errore nella fork\n");
				exit(-1);
			}
			// fai gestire il client al figlio
			if(pid==0) {
				printf("\n\t@FIGLIO %d [PID: %d; PID padre: %d], gestisco messaggio:\n", child_counter, getpid(), getppid());
				gestisci_messaggio(child_counter, buff, rcvd_bytes, (struct sockaddr *)&addr, len, window_size, p_loss, timeout_user);
				printf("\t@FIGLIO %d [PID: %d; PID padre: %d], ho svolto il mio compito, termino.\n\n", child_counter, getpid(), getppid());
				// SEMAFORO: figlio segnala che ha finito
				semop(id_semaforo, &segnala, 1);
				exit(0);
			}
			// padre preparati a metterti in ascolto di una nuova richiesta da un client
			if(pid>0) {
				// elimina dalla memoria il pkt ricevuto
				memset((void *)buff, 0, sizeof(buff));
			}
		}
	}
}


