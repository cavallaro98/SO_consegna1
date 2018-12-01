/* Cavallaro Salvatore 01/04/1998 matricola: O46001634 email: salvatorecavallaro98@gmail.com
Ho implementato questo algoritmo in modo tale che dato un path_name scansione tutto il contenuto delle directory e sottodirectory e li inserisce in una  lista. Attenzione se ci sono cartella vuote con nessun file all'interno, la directory non è inserita all'interno della lista, devo implementare anche
questa opzione.
Un altro problema di questo algoritmo, è che non classifica le cartelle di "sistema" come directory quindi non vengono scansionate.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h> //aprire un flusso di directory
#include <sys/types.h>
#include <sys/stat.h>

char path[100];

typedef struct s1{
	char nome_file[100];
	struct stat stat_file; //statistiche file
	char path_file[50];
}file_struct;
typedef struct n1{
	file_struct info;
	struct n1 *next;
}nodo_file, *p_lista_file;

p_lista_file lista_file;

//THREAD
void *thread_function(void *arg);

//MUTEX
pthread_mutex_t mutex_lista;

//Funzioni
int inserisci_lista(p_lista_file *lista, struct dirent *file, char *path_corrente);
void stampa_lista(p_lista_file lista_stampa);
p_lista_file trova_directory(p_lista_file lista);
int elimina_directory_lista(p_lista_file *lista);

p_lista_file scansiona_directory(char *path_name_directory);
int unisci_liste(p_lista_file *lista_principale, p_lista_file lista_da_unire);

int inserimento_directory_vuota(p_lista_file *lista_principale, char *path_corrente);


int main(){
	DIR *directory; // rappresenta il tipo di dati di un flusso di directory
	struct dirent *file;
	struct stat my_stat;
	lista_file = NULL; //inizializzo lista
	int numero_core = 2;
	//THREAD
	pthread_t thread[2];
	
	//MUTEX
	pthread_mutex_init(&mutex_lista, NULL);
	

	printf("\nInserisci il path: ");
	//fflush(stdout);
	//gets(&path);
	scanf("%s",&path);
	
	pthread_mutex_lock(&mutex_lista); //blocco il mutex

	int controllo=-1;

	if((directory = opendir(path)) != NULL){ //apre e restituisce un flusso di directory
		while(( file = readdir(directory)) != NULL){
			controllo = inserisci_lista(&lista_file, file, path);
		}
		closedir(directory);
	}else{
		perror("\nErrore apertura percorso");
	}

	for(int i=0; i<numero_core; i++){
		pthread_create(&thread[i], NULL, thread_function, NULL);
	}

	pthread_mutex_unlock(&mutex_lista);
	//sleep(1);
	for(int i=0; i<numero_core; i++){
		pthread_join(thread[i], NULL);
	}
	stampa_lista(lista_file);
	pthread_mutex_destroy(&mutex_lista);
}

void *thread_function(void *arg){

	p_lista_file directory;
	int trovato;
	int uscita =0;
	int unione_riuscita=0;
	while(!uscita){
		pthread_mutex_lock(&mutex_lista);
		//printf("\n---Sono il thread--\n");
		directory = trova_directory(lista_file);
		if(directory != NULL){
			trovato = elimina_directory_lista(&lista_file);
			//printf("\n Trova directory mi ha tornato %s come una direcotry", directory->info.nome_file);
			directory->next=NULL;
			//printf("\n\n Directory salvata:\n");
			//stampa_lista(directory);
			pthread_mutex_unlock(&mutex_lista);
			
			/* Adesso devo leggere il contenuto della mia directory e poi inserirla in lista */
			p_lista_file subdirectory;
			char path_subdirectory[100];
			strcpy(path_subdirectory, path);
			strcat(path_subdirectory, "/");
			strcat(path_subdirectory, directory->info.nome_file);
			//printf("\n PATH: %s", path_subdirectory);
			subdirectory = scansiona_directory(path_subdirectory);
			//p_lista_file end_subdirectory;
			//end_subdirectory = trova_ultimo_elemento(subdirectory);
			
			while(!unione_riuscita){
				pthread_mutex_lock(&mutex_lista);
				int esito;
				
				if(subdirectory == NULL){
					//Inserimento directory vuota in lista
					//printf("\n -------------------------------------- %s", path_subdirectory);
					//inserimento_directory_vuota(&lista_file, path_subdirectory);
					
					unione_riuscita =1;
					pthread_mutex_unlock(&mutex_lista);
				}else{
					esito = unisci_liste(&lista_file, subdirectory);
					pthread_mutex_unlock(&mutex_lista);
					unione_riuscita=1;
				}
			}
			unione_riuscita=0;

		}else{
			uscita =1;
			pthread_mutex_unlock(&mutex_lista);
		}
	}
	pthread_exit(NULL);
	
}

int inserisci_lista(p_lista_file *lista, struct dirent *file, char *path_corrente){
	char nome[50];
	strcpy(nome, file->d_name);
	
	if( (*lista) == NULL){	
		if( strcmp( nome, ".") != 0 && strcmp( nome, "..") != 0){
			p_lista_file aux;
			aux = (p_lista_file)malloc(sizeof(nodo_file));
			strcpy(aux->info.nome_file, file->d_name);
			strcpy(aux->info.path_file, path_corrente);
			stat(file->d_name, &aux->info.stat_file); 
			/* Questa istruzione mi permette di assegnare al mio tipo di dato all'interno della mia lista
			le informazion sul file che sto inserendo in lista */
			aux->next = *lista;
			*lista = aux;
			return 1;
		}
	}

	
	if( strcmp( nome, ".") != 0 && strcmp( nome, "..") != 0){
		if( strcmp( (*lista)->info.nome_file, nome) > 0 ){
			//Inserimento in testa	
			p_lista_file aux;
			aux = (p_lista_file)malloc(sizeof(nodo_file));
			strcpy(aux->info.nome_file, nome);
			strcpy(aux->info.path_file, path_corrente);
			stat(file->d_name, &aux->info.stat_file);
			aux->next = *lista;
			*lista = aux;
			return 1;
		}else if( strcmp( (*lista)->info.nome_file, nome) < 0 ){
			// richiamo la mia stessa funzione ricorsivamente
			inserisci_lista( &(*lista)->next, file, path_corrente);
		}
	}
	return 0;
}


void stampa_lista(p_lista_file lista_stampa){
	printf("\n");
	while( lista_stampa != NULL){
		printf("\n|Nome_file = %s ",lista_stampa->info.nome_file);
		//printf("\n|					|| Dimensioni_file: %ld", lista_stampa->info.stat_file.st_size);
		printf("\n|					|| Path_file: %s", lista_stampa->info.path_file);
		if(S_ISDIR(lista_stampa->info.stat_file.st_mode)){
			//CONTROLLA se è una directory
			printf("\n|					|| E' una directory");
		}
		lista_stampa = lista_stampa->next;	
	}
	printf("\n");
}

p_lista_file trova_directory(p_lista_file lista){
	while(lista != NULL){
		if(S_ISDIR(lista->info.stat_file.st_mode)){
			//printf("\nil file %s e' una directory\n", lista->info.nome_file);
			return lista;
		}else{
		lista = lista->next;
		}
	}
	return NULL;
}

int elimina_directory_lista(p_lista_file *lista){
	while(*lista != NULL){
		if(S_ISDIR((*lista)->info.stat_file.st_mode)){
			//printf("\nil file %s e' una directory\n", lista->info.nome_file);
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
	struct stat my_stat;
	int controllo;
	p_lista_file subdirectory = NULL;

	if((directory = opendir(path_name_directory)) != NULL){ //apre e restituisce un flusso di directory
		while(( file = readdir(directory)) != NULL){
			controllo = inserisci_lista(&subdirectory, file, path_name_directory);
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
	if((*lista_principale) == NULL){
		return -1;
	}else if((*lista_principale)->next == NULL){
		(*lista_principale)->next = lista_da_unire;
		return 1;
	}
	return unisci_liste(&(*lista_principale)->next, lista_da_unire);
	
}



int inserimento_directory_vuota(p_lista_file *lista_principale, char *path_corrente){
	DIR *directory;
	struct dirent *file;
	
	//directory=opendir(path_corrente);
	//file = readdir(directory);

	p_lista_file aux;
	aux = (p_lista_file)malloc(sizeof(nodo_file));
	strcpy(aux->info.nome_file, "-- DIRECTORY_VUOTA --");
	strcpy(aux->info.path_file, path_corrente);
	//aux->info.stat_file.st_size = 0;
	//stat(file->d_name, &aux->info.stat_file);
	aux->next = *lista_principale;
	*lista_principale = aux;
	return 1;
}





