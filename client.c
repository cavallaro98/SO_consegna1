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

int download(int server_socket);
int menu();
void download_lista_file(int server_socket);
int invio_file_richiesto(int client_descriptor, char *nome_file, char *path_file);

int main(void){
	int server_socket;
	struct sockaddr_in server_address;
	char buffer[100];
	char user[20];
	char password[20];
	char risposta;
	int utente_registrato = 0;
	char ricevuto;
	int success;
	int scelta;
	char nome_file[20];
	char path_file[100];
	char cmd_shell[20];
	int n_cmd_shell;
	char option_cmd[10];

	system("clear");
	//creo la socket
	server_socket = socket(AF_INET, SOCK_STREAM,0);

	/*Creo la stuttura con indirizzo ip/porta del server*/
	server_address.sin_family = AF_INET; //IPv4
	server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); //indirizzo ip; accetto connessioni da tutti
	server_address.sin_port = htons(7779); //porta

	connect(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
	printf("\n______________CONNESSO AL SERVER______________\n\n");
	read(server_socket, buffer, 31);
	printf("\n%s", buffer);
	memset(buffer, 0, strlen(buffer));

	scanf("%c", &risposta);
	write(server_socket, &risposta, 1);

	if(risposta == 'y' || risposta == 'Y'){
		read(server_socket, buffer, 39);
		printf("%s", buffer);
		memset(buffer, 0, strlen(buffer));
		scanf("%s", user);
		write(server_socket, user, 20);
		read(server_socket, buffer, 10);
		printf("%s", buffer);
		memset(buffer, 0, strlen(buffer));
		scanf("%s", password);
		write(server_socket, password, 20);
		
		read(server_socket, buffer, 19);
		printf("\n_______________________________________________________");
		printf("\n%s", buffer);
		printf("\n_______________________________________________________\n");
		//memset(buffer, 0, strlen(buffer));

	}
	if(strncmp(buffer, "Utente NON trovato", 19) == 0){
		memset(buffer, 0, strlen(buffer));
		utente_registrato = 0;
		
	}else if(strncmp(buffer, "__Utente trovato___", 19) == 0){
		memset(buffer, 0, strlen(buffer));
		utente_registrato = 1;
	}

	if(risposta == 'n' || risposta == 'N' || utente_registrato == 0){
		printf("\nRichiesta di registrazione inviata.. attendi..\n");
		fflush(stdout);
		read(server_socket, buffer, 39);
		printf("%s", buffer);
		
		if((strncmp(buffer, "Richiesta di registrazione accettata!!", 39)) == 0){
			memset(buffer, 0, strlen(buffer));
			read(server_socket, buffer, 71);
			printf("%s", buffer);
			memset(buffer, 0, strlen(buffer));
			scanf("%s", user);
			write(server_socket, user, 20);
			read(server_socket, buffer, 63);
			printf("%s", buffer);
			fflush(stdout);
			if(strncmp(buffer, "Password: ", 10) == 0){
				scanf("%s", password);
				write(server_socket, password, 20);
			}
			memset(buffer, 0, strlen(buffer));
		}else{
			close(server_socket);
			exit(EXIT_FAILURE);
		}
	}

	while((scelta = menu()) != 0){
		switch(scelta){
			case 1:
				write(server_socket, "_download_", 10);
				download_lista_file(server_socket);
				success = download(server_socket);
				if(success == 0) printf("\n-- File ricevuto con successo --");
				if(success == -1) printf("\n-- Errore recezione file --");
			break;
			case 2:	
				write(server_socket, "__upload__", 10);
				read(server_socket, &ricevuto, 1);
				memset(nome_file, 0, 20);
				memset(path_file,0, 100);
				printf("Digita il nome del file da inviare: ");
				scanf("%s*c", nome_file);
				printf("Digita il path corrente del file da inziare: ");
				scanf("%s*c", path_file);
				printf("\nIl file inviato vine memorizzato sul server nel seguente path: /home/<nome_utente>/Scaricati\n");
				fflush(stdout);
				write(server_socket, nome_file, strlen(nome_file));
				read(server_socket, &ricevuto, 1);
				success = invio_file_richiesto(server_socket, nome_file, path_file);
				if(success == 0){
					printf("\n__FILE INVIATO CON SUCCESSO__\n");
				}else{
					printf("\n__INVIO FILE FALLITO__\n");
				}
			break;
			case 3:
				memset(cmd_shell, 0, 20);
				memset(option_cmd,0, 10);
				//strcpy(option_cmd, "0");
				write(server_socket, "cmd_shell_", 10);	
				read(server_socket, &ricevuto, 1);
				printf("Digita il comando da eseguire su shell remota: ");
				scanf("%s*c", cmd_shell);
				printf("[\"0\": NESSUN OPZIONE]Digita un eventuale opzione del comdano: ");
				scanf("%s*c", option_cmd);
				if(strcmp(option_cmd, "0") == 0){
					printf("\nNessun opzione digitata\n");
				}else{
					strcat(cmd_shell, " ");
					strcat(cmd_shell, option_cmd);
				}
				printf("\n_______________________________________________________\n");
				n_cmd_shell = strlen(cmd_shell);
				write(server_socket, &n_cmd_shell, sizeof(int));
				read(server_socket, &ricevuto, 1);
				write(server_socket, cmd_shell, n_cmd_shell);
				while(1){
					read(server_socket ,buffer, 100);
					printf("%s", buffer);
					fflush(stdout);
					//sleep(1);
					if(strncmp(buffer, "_end_to_stream_", 15) == 0){
						printf("\n_______________________________________________________\n");
						fflush(stdout);
						break;
					}
					memset(buffer, 0, strlen(buffer));
				}
			break;
		}
	}
	write(server_socket, "___close__", 10);
	close(server_socket);
	exit(EXIT_SUCCESS);
}

int download(int server_socket){
	//Invio richiesta upload
	char nome_file[20];
	char path_file[100];
	char ricevuto;

	memset(nome_file, 0, 20);
	memset(path_file,0, 100);
	printf("Digita il nome del file da ricevere: ");
	scanf("%s", nome_file);
	printf("Digita il path corrente del file da ricevere: ");
	scanf("%s", path_file);
	//printf("%s -- %s", nome_file, path_file);
	write(server_socket, nome_file, strlen(nome_file));
	read(server_socket, &ricevuto, 1);
	write(server_socket, path_file, strlen(path_file));
	
	char recvBuff[1024];
	memset(recvBuff, '0', sizeof(recvBuff));
	int bytesReceived = 0;
	FILE *fp;
	printf("\nReceiving file...");
	fp = fopen(nome_file, "ab"); 
	if(NULL == fp){
		printf("\nErrore apertura file");
		return -1;
	}

	while((bytesReceived = read(server_socket, recvBuff, 1024)) > 0){
		if(strncmp(recvBuff, "Errore", 6) == 0){
			remove(nome_file);	
			return -1;
		}
		if(strncmp(recvBuff, "_fine_file_", 11) == 0){	
			//printf("\n\n__Fine file ricevuto__");
			fflush(stdout);
			fclose(fp);
			return 0;
		}
		if(bytesReceived < 1024){
			fwrite(recvBuff, 1, bytesReceived, fp);
			fclose(fp);
			return 0;
		}
		fwrite(recvBuff, 1,bytesReceived,fp);
	}
	
	if(bytesReceived > 0 && bytesReceived < 1024){
		fwrite(recvBuff, 1,bytesReceived,fp);
		//printf("\n\n--Fine File--");
	}
	if(bytesReceived < 0){
		return -1;
	}
	fclose(fp);
	return 0;
}

int menu(){
	int scelta;
	printf("\n_______________MENU'________________");
	printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	printf("\n0) Chiudi connessione              |");
	printf("\n1) Download file server            |");
	printf("\n2) Upload file server              |");
	printf("\n3) CMD Shell remota                |");
	printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	printf("\n[SELEZIONA UNA DELLE SEGUENTI OPZIONI]: ");
	scanf("%d", &scelta);
	return scelta;
}

void download_lista_file(int server_socket){
	char buffer[100];

	read(server_socket, buffer, 25);
	printf("\n%s", buffer);	
	fflush(stdout);
	memset(buffer, 0, strlen(buffer));
	while(1){
		read(server_socket, buffer, 100);
		if(strncmp(buffer, "Fine_trasmissione_lista", 23) == 0){	
			fflush(stdout);		
			break;
		}
		printf("\nNome_file = %s	|| Path_file = ", buffer);
		memset(buffer, 0, strlen(buffer));
		write(server_socket, "r",1);
		read(server_socket, buffer, 100);
		printf("%s", buffer);
		fflush(stdout);
		write(server_socket, "r", 1);
		memset(buffer, 0, strlen(buffer));
	}
	printf("\n\n%s\n", buffer);
	fflush(stdout);
	memset(buffer, 0, strlen(buffer));
}

int invio_file_richiesto(int client_descriptor, char *nome_file, char *path_file){
	strcat(path_file, "/");
	strcat(path_file, nome_file);
	printf("\n[File selezionato]: %s", path_file);
	
	FILE *fp = fopen(path_file,"rb");
	if(fp==NULL){
		write(client_descriptor, "Errore", 6);
		perror("\nErrore apertura file");
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
				//printf("\n-- Fine file --");
				//write(client_descriptor, "_fine_file_",11);
				break;
			}else{
				write(client_descriptor, buff, nread);
				//break;
			}
		}
	}
	
	fclose(fp);

	return 0;
}








