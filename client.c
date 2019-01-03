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

int main(void){
	int server_socket;
	struct sockaddr_in server_address;
	char buffer[100];
	char user[20];
	char password[20];
	char risposta;
	int utente_registrato = 0;
	char ricevuto;
	system("clear");
	//creo la socket
	server_socket = socket(AF_INET, SOCK_STREAM,0);

	/*Creo la stuttura con indirizzo ip/porta del server*/
	server_address.sin_family = AF_INET; //IPv4
	server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); //indirizzo ip; accetto connessioni da tutti
	server_address.sin_port = htons(7779); //porta

	connect(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
	read(server_socket, buffer, 31);
	printf("%s", buffer);
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
		printf("%s", buffer);
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
		}
	}
	
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

	//Invio richiesta upload
	char nome_file[20];
	char path_file[100];
	memset(nome_file, 0, 20);
	memset(path_file,0, 100);
	printf("Digita il nome del file da ricevere: ");
	scanf("%s", nome_file);
	printf("Digita il path corrente del file da ricevere: ");
	scanf("%s", path_file);
	printf("%s -- %s", nome_file, path_file);
	write(server_socket, nome_file, strlen(nome_file));
	read(server_socket, &ricevuto, 1);
	write(server_socket, path_file, strlen(path_file));


	char recvBuff[1024];
	memset(recvBuff, '0', sizeof(recvBuff));
	int bytesReceived = 0;
	FILE *fp;
	printf("Receiving file...");
	fp = fopen(nome_file, "ab"); 
	if(NULL == fp){
		printf("\nErrore apertura file");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	while((bytesReceived = read(server_socket, recvBuff, 1024)) > 0){ 
		fwrite(recvBuff, 1,bytesReceived,fp);
	}

	if(bytesReceived < 0){
		printf("\n Read Error \n");
	}
	printf("\n-- File ricevuto con successo --");
	fflush(stdout);

	close(server_socket);
	exit(EXIT_SUCCESS);
}







