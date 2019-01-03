#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <dirent.h> //aprire un flusso di directory

int numero_core;
int controllo_uscita[20]; //credo che 20 core al massimo possono bastare

typedef struct s1{
	char *nome_file;
	char *path_file;
	int flag_type; // se flag_type == 0 è un file; se flag_type == 1 è una directory; se il flag_type == -1 allora è una directory vuota
}file_struct;
typedef struct n1{
	file_struct info;
	struct n1 *next;
}nodo_file, *p_lista_file;

/*Questa è la lista principale contenente tutti i miei file*/
p_lista_file lista_file;

//THREAD
void *thread_function(void *arg);

//MUTEX
pthread_mutex_t mutex_lista;
pthread_mutex_t mutex_controllo_uscita;

//Funzioni
int inserisci_lista(p_lista_file *lista, struct dirent *file, char *path_corrente, int flag);
void stampa_lista(p_lista_file lista_stampa);
p_lista_file trova_directory(p_lista_file lista);
int elimina_directory_lista(p_lista_file *lista);
p_lista_file scansiona_directory(char *path_name_directory);
int unisci_liste(p_lista_file *lista_principale, p_lista_file lista_da_unire);
int inserimento_directory_vuota(p_lista_file *lista_principale, char *path_corrente);

//--------------------------------------------------------------------------------------------------------------------------
typedef struct client{
	int client_descriptor;
	struct sockaddr_in client_address;
}data_client;

void *send_client(void *arg);
FILE *fp_login;

int login(int client_descriptor, struct sockaddr_in client_address);
int controllo_utente_presente(char *user, int client_descriptor);
int richiesta_registrazione(int client_descriptor, struct sockaddr_in client_address);
int invio_lista(p_lista_file lista, int client_descriptor);
int invio_file_richiesto(int client_descriptor, char *nome_file, char *path_file);
int ls_home();

int main(){
	int server_socket;
	//int client_descriptor;
	struct sockaddr_in server_address;
	//struct sockaddr_in client_address;
	data_client client;
	int controllo;
	int n_connessioni = 0;
	pthread_t thread[5];
	size_t client_len = 0;
	int accesso;
	int file_login;
	int ls_success;

	/*Creo o apro il file login.txt */
	file_login = open("login.txt", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IXUSR);
	if(file_login != -1) close(file_login);
	
	printf("\nScansione \"home\" in corso...");
	ls_success = ls_home();
	printf("\nScansione completata con successo! = ls_success:%d", ls_success);

	/*Creo la socket*/
	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	/*Creo la stuttura con indirizzo ip/porta del server*/
	server_address.sin_family = AF_INET; //IPv4
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); //indirizzo ip; accetto connessioni da tutti
	server_address.sin_port = htons(7779); //porta

	/*Creo la bind, cosi da rendere il mio socket reperibile*/
	controllo = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
	if(controllo == -1){
		perror("\nErrore bind!!!");
		exit(EXIT_FAILURE);
	}

	/*Adesso metto il mio server in ascolto; imposto massimo 5 connessioni contemporaneamente*/
	listen(server_socket, 5);
	
	/*Adesso creo un loop infinito per il server*/
	while(1){
		client_len = sizeof(client.client_address);
		client.client_descriptor = accept(server_socket, (struct sockaddr*)&client.client_address, &client_len);
		pthread_create(&thread[n_connessioni], NULL, send_client, (void*)&client);
		n_connessioni ++;
		
	}
}

int login(int client_descriptor, struct sockaddr_in client_address){
	char buff[20];
	char buff_user[20];
	char buff_password[20];
	char user[20];
	char password[20];
	char utente;
	int utente_trovato = 0;
	int utente_registrato = 0;
	FILE *f_login;
	
	int file_login;
	

	write(client_descriptor, "Sei un utente registato?[Y/N]: ", 31);
	read(client_descriptor, &utente, 20);
	
	if(utente == 'y' || utente == 'Y'){
		f_login = fopen("login.txt", "r");
		if(f_login ==  NULL) printf("\nErrore");

		write(client_descriptor, "Inserisci le tue credenziali\nUsername: ", 39);
		read(client_descriptor, user, 20);
		printf("\n-- User: %s", user);
		fflush(stdout);
		write(client_descriptor, "Password: ", 10);
		read(client_descriptor, password, 20);
		printf("\n-- Password: %s", password);
		fflush(stdout);

		while( (fscanf(f_login, "%s %s %c %s %s", buff, buff_user, buff, buff, buff_password)) == 5 ){
			if( (strcmp(buff_user, user) == 0) && (strcmp(buff_password, password) == 0) ){
				write(client_descriptor, "__Utente trovato___", 19);
				printf("\n__Utente trovato___");
				return 0;
				//utente_trovato = 1;
				//utente_registrato = 1;
			}
		}
		if(utente_trovato == 0){
			write(client_descriptor, "Utente NON trovato", 19);
		}
		
		fclose(f_login);
	}
	
	while(!utente_registrato){
		if(utente == 'n' || utente == 'N' || utente_trovato == 0){
			int controllo;
			int accettazione;
			accettazione = richiesta_registrazione(client_descriptor, client_address);
			if(accettazione == -1){
				write(client_descriptor, "Richiesta di registrazione rifiutata!!", 39);
				return -1;
			}else if(accettazione == 0){
				write(client_descriptor, "Richiesta di registrazione accettata!!", 39);
				write(client_descriptor,"\n__RICHIESTA DI REGISTRAZIONE__\nInserisci le tue credenziali\nUsername: ", 71);
				read(client_descriptor, user, 20);
				printf("\nUser:%s", user);
				controllo = controllo_utente_presente(user, client_descriptor);
				printf("\nControllo User presente: %d", controllo);
				if(controllo == 0){	
					write(client_descriptor, "Password: ", 11);
					read(client_descriptor, password, 20);
					printf("\n-- User non presente --");
					printf("\nPASSWORD: %s", password);
					printf("\nREGISTRAZIONE IN CORSO...");
					f_login = fopen("login.txt", "a");
					fprintf(f_login, "User: %s | password: %s\n", user, password);
					printf("\n__ REGISTRAZIONE COMPLETATA __");
					fclose(f_login);
					utente_registrato = 1;
				}
			}	
		}
	}
	
}

int controllo_utente_presente(char *user, int client_descriptor){
	FILE *f_login;
	char buff[20];
	char buff_user[20];
	f_login = fopen("login.txt", "r");
	if(f_login ==  NULL) printf("\nErrore");						
	while( (fscanf(f_login, "%s %s %c %s %s", buff, buff_user, buff, buff, buff)) == 5 ){
		if( (strcmp(buff_user, user) == 0) ){
			write(client_descriptor, "\n-- Username non disponibile --\n__Utilizza un altro username__\n", 63);
			fclose(f_login);
			return 1;
		}
	}
	fclose(f_login);
	return 0;
}

void *send_client(void *arg){
	data_client client = *((data_client*)arg);
	
	int accesso;
	int ctrl_invio_lista;

	printf("\nRichiesta accesso: \n- indirizzo ip_client: %s \n- porta_client:%d ", inet_ntoa(client.client_address.sin_addr), ntohs(client.client_address.sin_port));
	printf("\nControllo autentificazione ...");
	fflush(stdout);
	accesso = login(client.client_descriptor, client.client_address);
	if(accesso == -1){
		printf("\nAutentificazione fallita!!!");
		fflush(stdout);
		close(client.client_descriptor);
		exit(EXIT_FAILURE);
	}else{
		printf("\nAutentificazione accettata -- Connessione stabilita --");
		ctrl_invio_lista = invio_lista(lista_file, client.client_descriptor);
		if(ctrl_invio_lista == 0){
			printf("\nInvio lista completato con successo");
			printf("\nAttendo richiesta di invio...");
			char nome_file[20];
			char path_file[100];
			memset(nome_file, 0, 20);
			read(client.client_descriptor, nome_file, 20);
			write(client.client_descriptor, "r", 1);
			printf("\nnome:%s", nome_file);
			memset(path_file, 0, 100);
			read(client.client_descriptor, path_file, 100);
			printf("\npath:%s", path_file);
			int invio_riuscito;
			invio_riuscito = invio_file_richiesto(client.client_descriptor, nome_file,path_file);
			printf("\nInvio riuscito = %d", invio_riuscito);
			close(client.client_descriptor);
		}else if(ctrl_invio_lista == -1){
			printf("\nInvio lista fallito");
		}
		fflush(stdout);
	}		
	
}


int richiesta_registrazione(int client_descriptor, struct sockaddr_in client_address){
	char accettazione = ' ';	
	printf("\n--------------------------------------------------------");
	printf("\n-             __RICHIESTRA REGISTRAZIONE__             -");
	printf("\n- IP_client: %s port_client: %d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
	printf("\n- ACCETTARE REGISTRAZIONE[Y/N]:");
	scanf("%c", &accettazione);
	printf("\n--------------------------------------------------------");
	if(accettazione == 'y' || accettazione == 'Y') return 0;
	if(accettazione == 'n' || accettazione == 'N') return -1;
}

//--------------------------------------------------------------------------------------------------------------------------

int ls_home(){
	DIR *directory; // rappresenta il tipo di dati di un flusso di directory
	struct dirent *file;
							
	//int directory_nascosta;
	int flag;
	int controllo;
	int opzioni_menu;
	
	numero_core = get_nprocs_conf();

	//THREAD
	pthread_t thread[numero_core];
	
	//MUTEX
	pthread_mutex_init(&mutex_lista, NULL);
	pthread_mutex_init(&mutex_controllo_uscita, NULL);
	
	/* Il vettore 'controllo_uscita' ha il compito di tenere in vita tutti i thread fin quando la lista
	non e' completamente priva di directory, senza questo controllo può capitare che un thread muore perche'
	crede la lista priva di directory pero ancora l'altro thread deve aggiungere la lettura di una directory che 
	aveva rimosso e quindi può trovare molteplici directory però in questo momento ormai il 'fretello' thread ha gia 
	terminato, in questo modo mi assicuro che i thread terminano insieme solo quando la lista e' stata completata.*/
	for(int i=0; i<numero_core; i++){
		controllo_uscita[i]=0;	
	}
	
	/* Blocco il mutex della lista ache se non ce ne realmente bisogno perche' per adesso esiste solo il main_thread*/
	pthread_mutex_lock(&mutex_lista);

	if((directory = opendir("/home")) != NULL){ //apre e restituisce un flusso di directory
		while(( file = readdir(directory)) != NULL){
			if(file->d_type == DT_DIR){
				flag=1; //il flag settato a 1 rappresenta una directory
			}else{
				flag=0; //il flag settato a 0 rappresenta un file qualsiasi
			}
			/*if(strncmp(file->d_name, ".",1) == 0){
				directory_nascosta = 1;
			}else{
				directory_nascosta = 0;
			}*/
			controllo = inserisci_lista(&lista_file, file, "/home", flag);
		}
		closedir(directory);
	}else{
		perror("\n-- ERRORE APERTURA PERCORSO --");
		return -1;
	}
	
	/*Adesso creo tanti thread in base al mio numero di core*/
	for(int i=0; i<numero_core; i++){
		pthread_create(&thread[i], NULL, thread_function,(void *)(intptr_t)i);
	}

	pthread_mutex_unlock(&mutex_lista);
	
	printf("\nRICERCA IN CORSO...");
	fflush(stdout);
	/*Il main thread non deve fare più nulla per adesso quindi deve attendere il completamente del lavore dei thread e li aspetta*/
	for(int i=0; i<numero_core; i++){
		pthread_join(thread[i], NULL);
	}
	
	printf("\nRICERCA COMLETATA CON SUCCESSO\n");
	fflush(stdout);
	/*Quando i thread hanno finito di comporre la lista e hanno terminato allora il main thread stampa a video la nuova lista creata dai thread*/
	stampa_lista(lista_file);
	

	/*Adesso posso distruggere i mutex che ho usato*/
	pthread_mutex_destroy(&mutex_lista);
	pthread_mutex_destroy(&mutex_controllo_uscita);
	/*FINE*/
	return 0;
}

void *thread_function(void *arg){
	int numero_thread = (intptr_t)arg;

	p_lista_file directory;
	int trovato;
	int uscita =0;
	int unione_riuscita=0;
	int esito;
	//int directory_nascosta;

	while(!uscita){
		/* Il thread chiede l'accesso alla lista per trovare una directory*/
		pthread_mutex_lock(&mutex_lista);
		/*Funzione per trovare una directory all'interno della lista, ritorna NULL se non trova direcotry,
		se invece trova una directory si memorizza il puntatore a questa directory*/
		directory = trova_directory(lista_file);
		if(directory != NULL){
			/*Una volta torvata una directory e avarla memorizzata prima, la rimuove della lista*/
			trovato = elimina_directory_lista(&lista_file);
			directory->next=NULL; //IMPORTANTE
			/*Adesso posso rilasciare il mutex della lista in modo tale da dare la possibilita'
			ad un'altro thread di iniziare il suo lavoro sulla lista*/
			pthread_mutex_unlock(&mutex_lista);
			
			/*Ovvimanete inserisco il valore nel vettore == 0 perche' io ho preso una directory quindi la lista che legge
			il thread 'fratello' non e' la lista completa quindi se non ci sono directory lui non puo' terminare ma deve
			attendere che termino io e poi controllare la nuova lista. Quando tutti i thread torvano la lista senza directory
			allora possono terminare insieme*/
			pthread_mutex_lock(&mutex_controllo_uscita);
			controllo_uscita[numero_thread]=0;
			pthread_mutex_unlock(&mutex_controllo_uscita);

			/* Adesso devo leggere il contenuto della mia directory e costruisco una lista locale al thread per poi successivamente
			richiedere l'accesso alla lista ed unirla alla lista globale*/
			p_lista_file subdirectory;
			char *path_subdirectory;
			path_subdirectory =(char*)malloc((strlen(directory->info.path_file)+strlen(directory->info.nome_file)+2)*sizeof(char*));
			//COSTRUISCO IL NUOVO PATH DA SCANSIONARE
			strcpy(path_subdirectory, "");
			strcat(path_subdirectory, directory->info.path_file);
			strcat(path_subdirectory, "/");
			strcat(path_subdirectory, directory->info.nome_file);
			subdirectory = scansiona_directory(path_subdirectory);
			
			/*Una volta creata la lista con il contenuto della directory che ho scansionato posso prendere la directory 
			e distruggerla*/
			free(directory);

			while(!unione_riuscita){
				/*Adesso devo unire la lista della subdirectory che ha creato il thread alla lista 'GENERALE'
				quindi richiedo l'accesso alla lista tramite mutex*/
				pthread_mutex_lock(&mutex_lista);
				/*Controllo se la directory che ho scansionato e' vuota oppure ci sono file/directory all'interno,
				perche' tratto l'inserimento in lista diversamente*/
				if(subdirectory == NULL){
					inserimento_directory_vuota(&lista_file, path_subdirectory);
					unione_riuscita =1;
					pthread_mutex_unlock(&mutex_lista);
				}else{
					esito = unisci_liste(&lista_file, subdirectory);
					pthread_mutex_unlock(&mutex_lista);
					unione_riuscita=1;
				}
			}
			unione_riuscita=0; //resetto la condizione

		}else{
			/*Quando la lista sarà priva di directory allora i thread setteranno il loro stato di uscita == 1
			e controllano la somma degli elementi contenuti nel vettore, se e' pari al numero di core allora significa
			che tutti i thread non hanno rilevato directory nella lista e nessun thread sta lavorando quindi possono terminare*/
			pthread_mutex_lock(&mutex_controllo_uscita);
			controllo_uscita[numero_thread]=1;
			int somma=0;
			for(int i=0; i<numero_core; i++){
				if(controllo_uscita[i] ==1) somma=somma+1;
			}
			if(somma == numero_core) uscita =1;
			else uscita=0;
			pthread_mutex_unlock(&mutex_controllo_uscita);
			pthread_mutex_unlock(&mutex_lista);
		}
	}
	pthread_exit(NULL);
	
}

int inserisci_lista(p_lista_file *lista, struct dirent *file, char *path_corrente, int flag){
	/*Classica funzione di inserimento in lista. Inserisco in lista tutti gli elementi tranne le cartelle '.' e '..'; Inserimento in ordine 
	alfabetico SOLO per i file contenuti dentro le direcoty */
	char *nome;
	nome = (char*)malloc((strlen(file->d_name)+1)*sizeof(char));
	strcpy(nome, file->d_name);

	if( (*lista) == NULL){	
		if( strcmp( nome, ".") != 0 && strcmp( nome, "..") != 0 && strncmp(nome, ".",1) != 0){
			p_lista_file aux;
			aux = (p_lista_file)malloc(sizeof(nodo_file));
			aux->info.nome_file = nome;
			aux->info.path_file = path_corrente;
			//stat(file->d_name, &aux->info.stat_file); 
			/* Questa istruzione mi permette di assegnare al mio tipo di dato all'interno della mia lista
			le informazion sul file che sto inserendo in lista */
			aux->info.flag_type = flag;
			/*if(directory_nascosta == 0){
				if(strncmp(file->d_name, ".",1) == 0){
					aux->info.flag_directory_nascosta = 1;
				}else{
					aux->info.flag_directory_nascosta = 0;
				}
			}else{
				aux->info.flag_directory_nascosta = directory_nascosta;
			}*/
			aux->next = *lista;
			*lista = aux;
			return 1;
		}
	}

	
	if( strcmp( nome, ".") != 0 && strcmp( nome, "..") != 0 && strncmp(nome, ".",1) != 0){
		if( strcmp( (*lista)->info.nome_file, nome) > 0 ){
			//Inserimento in testa	
			p_lista_file aux;
			aux = (p_lista_file)malloc(sizeof(nodo_file));
			aux->info.nome_file = nome;
			aux->info.path_file = path_corrente;
			//stat(file->d_name, &aux->info.stat_file);
			aux->info.flag_type = flag;
			/*if(directory_nascosta == 0){
				if(strncmp(file->d_name, ".",1) == 0){
					aux->info.flag_directory_nascosta = 1;
				}else{
					aux->info.flag_directory_nascosta = 0;
				}
			}else{
				aux->info.flag_directory_nascosta = directory_nascosta;
			}*/
			aux->next = *lista;
			*lista = aux;
			return 1;
		}else if( strcmp( (*lista)->info.nome_file, nome) < 0 ){
			// richiamo la mia stessa funzione ricorsivamente
			inserisci_lista( &(*lista)->next, file, path_corrente, flag);
		}
	}
	return 0;
}


void stampa_lista(p_lista_file lista_stampa){

	printf("\n");
	while( lista_stampa != NULL){	
		printf("\n------------------------------------------------------------------------------------------------");
		printf("\n|Nome_file = %s ",lista_stampa->info.nome_file);
		printf("\n|					|| Path_file: %s", lista_stampa->info.path_file);
		lista_stampa = lista_stampa->next;	
	}
	printf("\n------------------------------------------------------------------------------------------------");
	printf("\n\n");	
}


p_lista_file trova_directory(p_lista_file lista){
	while(lista != NULL){
		if(lista->info.flag_type == 1){
			return lista;
		}else{
		lista = lista->next;
		}
	}
	return NULL;
}

int elimina_directory_lista(p_lista_file *lista){
	while(*lista != NULL){
		if((*lista)->info.flag_type == 1){
			*lista = (*lista)->next;
			return 1;
		}else{
			return elimina_directory_lista(&(*lista)->next);
		}
	}
	return 0;
}

p_lista_file scansiona_directory(char *path_name_directory){
	DIR *directory;
	struct dirent *file;
	int controllo;
	int flag;
	p_lista_file subdirectory = NULL;

	if((directory = opendir(path_name_directory)) != NULL){ //apre e restituisce un flusso di directory
		while(( file = readdir(directory)) != NULL){
			if(file->d_type == DT_DIR){
				flag=1;
			}else{
				flag=0;
			}
			controllo = inserisci_lista(&subdirectory, file, path_name_directory, flag);
		}
		closedir(directory);
	}else{
		//perror("\nErrore apertura percorso");
	}

	if( subdirectory != NULL ){
		return subdirectory;
	}

	return NULL;
}

int unisci_liste(p_lista_file *lista_principale, p_lista_file lista_da_unire){
	/*La funzione unisci_liste non fa altro che aggingere la lista creata dal thread in coda alla lista principale, il problme e'
	che perde 'molto' tempo per trovare la fine dell'ultimo elemento in lista, quindi potrebbe essere meglio implementare una coda
	invece di una lista in modo tale da rendere l'unione delle due liste 'molto' piu' efficiente*/
	if((*lista_principale) == NULL){
		*lista_principale=lista_da_unire;
		return 1;
	}else if((*lista_principale)->next == NULL){
		(*lista_principale)->next = lista_da_unire;
		return 1;
	}
	return unisci_liste(&(*lista_principale)->next, lista_da_unire);
	
}



int inserimento_directory_vuota(p_lista_file *lista_principale, char *path_corrente){
	/*Crea un elemento 'inutile' nella lista perche' simula un file di nome '-- DIRECTORY_VUOTA --' in modo tale 
	da indentificare una directory vuota*/	
	//DIR *directory;
	//struct dirent *file;
	p_lista_file aux;
	aux = (p_lista_file)malloc(sizeof(nodo_file));
	aux->info.nome_file = "-- DIRECTORY_VUOTA --";
	aux->info.path_file = path_corrente;
	//aux->info.stat_file.st_size = 0;
	//stat(file->d_name, &aux->info.stat_file); 
	aux->info.flag_type = -1;
	aux->next = *lista_principale;
	*lista_principale = aux;
	return 1;
}

int invio_lista(p_lista_file lista, int client_descriptor){
	char r;
	write(client_descriptor, "Inizio_trasmissione_lista", 25);
	while(lista != NULL){
		write(client_descriptor, lista->info.nome_file, strlen(lista->info.nome_file));
		read(client_descriptor, &r, 1);
		write(client_descriptor, lista->info.path_file, strlen(lista->info.path_file));
		read(client_descriptor, &r, 1);
		lista=lista->next;
	}
	write(client_descriptor, "Fine_trasmissione_lista", 23);
	return 0;
}

int invio_file_richiesto(int client_descriptor, char *nome_file, char *path_file){
	strcat(path_file, "/");
	strcat(path_file, nome_file);
	printf("\n%s", path_file);
	
	FILE *fp = fopen(path_file,"rb");
	if(fp==NULL){
		printf("\nFile opern error");
		return -1;   
	}   

	while(1){
		unsigned char buff[1024]={0};
		int nread = fread(buff,1,1024,fp);
		if(nread > 0){
			write(client_descriptor, buff, nread);
		}
		if (nread < 1024){
			if (feof(fp)){
				printf("\n-- Fine file --");
			}
			break;
		}
	}
	return 0;
}









