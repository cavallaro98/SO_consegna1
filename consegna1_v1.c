/* Cavallaro Salvatore 01/04/1998 matricola: O46001634 email: salvatorecavallaro98@gmail.com */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h> //aprire un flusso di directory
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>



int numero_core;
int controllo_uscita[20]; //credo che 20 core al massimo possono bastare

typedef struct s1{
	char *nome_file;
	struct stat stat_file; //statistiche file
	char *path_file;
	int flag_type; // se flag_type == 0 è un file; se flag_type == 1 è una directory; se il flag_type == -1 allora è una directory vuota
	int flag_directory_nascosta; // se flag_directory_nascosta == 1 è una directory nascosta, 0 una directory normale
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
int inserisci_lista(p_lista_file *lista, struct dirent *file, char *path_corrente, int flag, int directory_nascosta);
void stampa_lista(p_lista_file lista_stampa, int opzione);
p_lista_file trova_directory(p_lista_file lista);
int elimina_directory_lista(p_lista_file *lista);
p_lista_file scansiona_directory(char *path_name_directory, int directory_nascosta);
int unisci_liste(p_lista_file *lista_principale, p_lista_file lista_da_unire);
int inserimento_directory_vuota(p_lista_file *lista_principale, char *path_corrente);
int stampa_menu();
void avvia_menu(int scelta);

int main(int argc, char *argv[1]){
	if(argc == 1){
		printf("\nIl seguente programma dato un path scansiona ricorsivamente la directory e le subdirectory.");
		printf("\nPercorso di ricerca non iserito!!\nRilancia il programma inserendo un path valido");
		printf("\nEXIT_FAILURE!!\n");
		exit(EXIT_FAILURE);
	}
	DIR *directory; // rappresenta il tipo di dati di un flusso di directory
	struct dirent *file;
	struct stat my_stat;
	
	lista_file = NULL; //inizializzo lista
	
	//int numero_core = 0;
	int directory_nascosta;
	int flag;
	int controllo;
	int opzioni_menu;
	//la funzione get_nprocs_conf(void) restituisce il numero di core
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
	
	/*NON permetto di scansionare la root */
	if(argv[1][0] == '/' && strlen(argv[1]) < 2){
		printf("\nIMPOSSIBILE SCANSIONARE LA ROOT -USA UN'ALTRO PERCORSO-\n");
		exit(EXIT_FAILURE);
	}
	/*Questo passaggio mi serve per leggere un path 'generico' cioe' anche un path con terminazione in '/' */
	if(argv[1][strlen(argv[1])-1] == '/'){
		argv[1][strlen(argv[1])-1] = '\0';
	}
	
	printf("\n-- Inizio la scansione dei file a partire dal seguente path: '%s' --\n", argv[1]);

	if((directory = opendir(argv[1])) != NULL){ //apre e restituisce un flusso di directory
		while(( file = readdir(directory)) != NULL){
			if(file->d_type == DT_DIR){
				flag=1; //il flag settato a 1 rappresenta una directory
			}else{
				flag=0; //il flag settato a 0 rappresenta un file qualsiasi
			}
			if(strncmp(file->d_name, ".",1) == 0){
				directory_nascosta = 1;
			}else{
				directory_nascosta = 0;
			}
			controllo = inserisci_lista(&lista_file, file, argv[1], flag, directory_nascosta);
		}
		closedir(directory);
	}else{
		perror("\n-- ERRORE APERTURA PERCORSO --");
		printf("\nEXIT_FAILURE!!\n");
		exit(EXIT_FAILURE);
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
	//stampa_lista(lista_file);
	
	/*Adesso il main thread dopo aver elaborato la scelta stampa a video il menu di selezione di visualizzazione della lista*/
	opzioni_menu = stampa_menu();
	avvia_menu(opzioni_menu);

	/*Adesso posso distruggere i mutex che ho usato*/
	pthread_mutex_destroy(&mutex_lista);
	pthread_mutex_destroy(&mutex_controllo_uscita);
	/*FINE*/
	exit(EXIT_SUCCESS);
}

void *thread_function(void *arg){
	int numero_thread = (intptr_t)arg;

	p_lista_file directory;
	int trovato;
	int uscita =0;
	int unione_riuscita=0;
	int esito;
	int directory_nascosta;

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
			subdirectory = scansiona_directory(path_subdirectory, directory->info.flag_directory_nascosta);
			
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

int inserisci_lista(p_lista_file *lista, struct dirent *file, char *path_corrente, int flag, int directory_nascosta){
	/*Classica funzione di inserimento in lista. Inserisco in lista tutti gli elementi tranne le cartelle '.' e '..'; Inserimento in ordine 
	alfabetico SOLO per i file contenuti dentro le direcoty */
	char *nome;
	nome = (char*)malloc((strlen(file->d_name)+1)*sizeof(char));
	strcpy(nome, file->d_name);

	if( (*lista) == NULL){	
		if( strcmp( nome, ".") != 0 && strcmp( nome, "..") != 0){// && strncmp(nome, ".",1) != 0){
			p_lista_file aux;
			aux = (p_lista_file)malloc(sizeof(nodo_file));
			aux->info.nome_file = nome;
			aux->info.path_file = path_corrente;
			stat(file->d_name, &aux->info.stat_file); 
			/* Questa istruzione mi permette di assegnare al mio tipo di dato all'interno della mia lista
			le informazion sul file che sto inserendo in lista */
			aux->info.flag_type = flag;
			if(directory_nascosta == 0){
				if(strncmp(file->d_name, ".",1) == 0){
					aux->info.flag_directory_nascosta = 1;
				}else{
					aux->info.flag_directory_nascosta = 0;
				}
			}else{
				aux->info.flag_directory_nascosta = directory_nascosta;
			}
			aux->next = *lista;
			*lista = aux;
			return 1;
		}
	}

	
	if( strcmp( nome, ".") != 0 && strcmp( nome, "..") != 0){// && strncmp(nome, ".",1) != 0){
		if( strcmp( (*lista)->info.nome_file, nome) > 0 ){
			//Inserimento in testa	
			p_lista_file aux;
			aux = (p_lista_file)malloc(sizeof(nodo_file));
			aux->info.nome_file = nome;
			aux->info.path_file = path_corrente;
			stat(file->d_name, &aux->info.stat_file);
			aux->info.flag_type = flag;
			if(directory_nascosta == 0){
				if(strncmp(file->d_name, ".",1) == 0){
					aux->info.flag_directory_nascosta = 1;
				}else{
					aux->info.flag_directory_nascosta = 0;
				}
			}else{
				aux->info.flag_directory_nascosta = directory_nascosta;
			}
			aux->next = *lista;
			*lista = aux;
			return 1;
		}else if( strcmp( (*lista)->info.nome_file, nome) < 0 ){
			// richiamo la mia stessa funzione ricorsivamente
			inserisci_lista( &(*lista)->next, file, path_corrente, flag, directory_nascosta);
		}
	}
	return 0;
}


void stampa_lista(p_lista_file lista_stampa, int opzione){
	if(opzione == 1){
		printf("\n");
		while( lista_stampa != NULL){
			if(lista_stampa->info.flag_directory_nascosta == 0){
				printf("\n------------------------------------------------------------------------------------------------");
				printf("\n|Nome_file = %s ",lista_stampa->info.nome_file);
				printf("\n|					|| Path_file: %s", lista_stampa->info.path_file);
				lista_stampa = lista_stampa->next;
			}else{
				lista_stampa = lista_stampa->next;
			}	
		}
		printf("\n------------------------------------------------------------------------------------------------");
		printf("\n\n");
	}
	int directory_nascoste_presenti = 0;
	if(opzione == 2){
		printf("\n");
		while( lista_stampa != NULL){
			if(lista_stampa->info.flag_directory_nascosta == 1){
				directory_nascoste_presenti = 1;
				printf("\n------------------------------------------------------------------------------------------------");
				printf("\n|Nome_file = %s ",lista_stampa->info.nome_file);
				printf("\n|					|| Path_file: %s", lista_stampa->info.path_file);
				lista_stampa = lista_stampa->next;
			}else{
				lista_stampa = lista_stampa->next;
			}	
		}
		if(directory_nascoste_presenti == 0){
			printf("\n------------------------------------------------------------------------------------------------");
			printf("\nNESSUNA DIRECTORY NASCOSTA PRESENTE IN LISTA");
		}
		printf("\n------------------------------------------------------------------------------------------------");
		printf("\n\n");
	}

	if(opzione == 3){	
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

p_lista_file scansiona_directory(char *path_name_directory, int directory_nascosta){
	DIR *directory;
	struct dirent *file;
	struct stat my_stat;
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
			controllo = inserisci_lista(&subdirectory, file, path_name_directory, flag, directory_nascosta);
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

int stampa_menu(){
	int scelta;
	printf("\n_______________________________ START_MENU' _______________________________");
	printf("\n1) Stampa il contenuto della directory escluso le directory nascoste");
	printf("\n2) Stampa solo le directory nascoste");
	printf("\n3) Stampa tutto il contenuto della directory");
	printf("\n0) EXIT");
	printf("\n_______________________________  END_MENU'  _______________________________");
	printf("\n\nInserisci l'opzione scelta: ");
	scanf("%d", &scelta);
	printf("\n");
	return scelta;
}

void avvia_menu(int scelta){
	int fine = 0;
	while(!fine){
		switch(scelta){
			case 0:
				printf("\n\n--  FINE  --\n\n");
				fine = 1;
				break;
			case 1:
				stampa_lista(lista_file, 1);
				scelta = stampa_menu();
				break;
			case 2:
				stampa_lista(lista_file, 2);
				scelta = stampa_menu();
				break;
			case 3:
				stampa_lista(lista_file, 3);
				scelta = stampa_menu();
				break;
		}
	}
}
