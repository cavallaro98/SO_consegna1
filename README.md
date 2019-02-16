# SO_consegna1

L'applicazione sviluppata implementa un servizio di file trasfer, sia dal server al client che viceversa, inoltre è stata implementata una funzione di controllo di Shell remota del server.
L’accesso al server è protetto da un basico sistema di login.
Il "Client" può far richiesta di registrazione verso il server e se verrà accettata dal "Server" (che a sua volta memorizza le credenziali d’accesso in un apposito file), può proseguire richiedendo al server una delle varie opzioni presenti nel menù di scelta.
Il menù è composto da 4 voci:
  - 0) Chiusura connessione 
  - 1) Download file dal server
  - 2) Upload file sul server
  - 3) Comandare Shell remota del server
Quando si richiede di scaricare file dal server, come prima cosa, esso invierà una lista di tutti i file presenti sul server dalla directory "/home/" in poi. Il client allora può richiedere al server il file desiderato.
L'opzione d’invio di un file verso il server viene gestita in modo analogo, il client deve selezionare il file da inviare al server, il quale attende l'invio del file che, una volta ricevuto, viene memorizzato nella directory "/home/<nome_utente>/Scaricati" del server.
L'ultima opzione prevede l'invio di un comando Shell da parte del client al server, il quale verrà eseguito sulla Shell del server e l'output verrà inviato al client richiedente.
È prevista, infine, un’opzione per la chiusura della connessione. 
