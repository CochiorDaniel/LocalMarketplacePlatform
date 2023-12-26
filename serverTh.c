#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h> 
#include <sqlite3.h>

/* portul folosit */
#define PORT 2909
#define MAX_COMMAND_LENGTH 1028

/* codul de eroare returnat de anumite apeluri */
extern int errno;

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

typedef struct {
    char command[50];
    char **params;      // Vector de șiruri pentru parametrii
    int numParams;  
} CommandParams;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);

void freeCommandParams(CommandParams *cmdParams) {
    
    for (int i = 0; i < cmdParams->numParams; i++) {
        free(cmdParams->params[i]);
    }
    free(cmdParams->params);
}

CommandParams parseCommand(char *input) {
    CommandParams result;

    result.params = NULL;
    result.numParams = 0;

    char *token = strtok(input, ":");

    strcpy(result.command, token);

    while ((token = strtok(NULL, " ")) != NULL) {
        result.params = realloc(result.params, (result.numParams + 1) * sizeof(char*));
        result.params[result.numParams] = strdup(token);
        result.numParams++;
    }

    return result;
}

char* exit_f(int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    sqlite3_stmt *stmt_upd;
    sqlite3 *db;
    int open_db = sqlite3_open("mkDB.db", &db);
    if(open_db != SQLITE_OK){
        printf("Eroare la deschiderea bazei de date!\n");
        exit(EXIT_FAILURE);
    }
    int res_upd;

    char *query2 = sqlite3_mprintf("UPDATE Useri SET logged = 0 WHERE id = '%d';", *id);
    res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
    sqlite3_step(stmt_upd);
    sqlite3_finalize(stmt_upd);
    sqlite3_close(db);
    strcpy(result, "exit");

    return result;
}

char* login_client(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);
    if(params.numParams != 2){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "login: <username> <password>\n");
        freeCommandParams(&params);
        return result;
    }
    
    params.params[1] = strtok(params.params[1], "\n");
    if(*id != -1){
        strcat(result, "Sunteti deja logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt, *stmt_upd;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db, res_upd;
        char *query1 = sqlite3_mprintf("SELECT * FROM Useri WHERE username = '%q' AND password = '%q';", params.params[0], params.params[1]);
        printf("%s\n", query1);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                *id = sqlite3_column_int(stmt, 0);
                char *query2 = sqlite3_mprintf("UPDATE Useri SET logged = 1 WHERE username = '%q';", params.params[0]);
                res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
                sqlite3_step(stmt_upd);
                sqlite3_finalize(stmt_upd);
                strcat(result, "V-ati logat cu succes!\n");
            }
            else{
                strcat(result, "Login esuat!\n");
            }
        }
        printf("id = %d\n", *id);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* creare_cont(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    CommandParams params = parseCommand(cmd);
    if(params.numParams != 3){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "creare cont: <username> <password> <sold>\n");
        freeCommandParams(&params);
        return result;
    }

    //strcpy(result, "Comanda <creare cont> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result, params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result, params.params[i]);
        strcat(result, "\n");
    }

    freeCommandParams(&params);
    return result;
}

char* lista_produse(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    strcpy(result, "Comanda <lista produse> a fost receptionata!");

    return result;
}

char* creare_oferta(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);
    if(params.numParams != 2){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "creare oferta: <nume_produs> <pret_produs>\n");
        freeCommandParams(&params);
        return result;
    }

    //strcpy(result, "Comanda <creare oferta> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result, params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result, params.params[i]);
        strcat(result, "\n");
    }

    freeCommandParams(&params);
    return result;
}

char* modificare_oferta(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 3){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "modificare oferta: <id> <nume_produs> <pret_produs>\n");
        freeCommandParams(&params);
        return result;
    }

    //strcpy(result, "Comanda <modificare oferta> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result, params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result, params.params[i]);
        strcat(result, "\n");
    }

    freeCommandParams(&params);
    return result;
}

char* stergere_oferta(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 1){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "stergere oferta: <id>\n");
        freeCommandParams(&params);
        return result;
    }

    //strcpy(result, "Comanda <stergere oferta> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result, params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result, params.params[i]);
        strcat(result, "\n");
    }

    // de verificat ca produsul sa nu fi fost cumparat
    freeCommandParams(&params);
    return result;
}

char* istoric_achizitii(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    strcpy(result, "Comanda <istoric achizitii> a fost receptionata!");

    // dupa id-ul clientului
    return result;
}

char* cumparare_produs(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 1){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "cumparare produs: <id>\n");
        freeCommandParams(&params);
        return result;
    }
    
    //strcpy(result, "Comanda <cumparare produs> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result, params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result, params.params[i]);
        strcat(result, "\n");
    }

    freeCommandParams(&params);
    return result;
}

char* cautare_produs(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    
    CommandParams params = parseCommand(cmd);

    if(params.numParams != 1){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "cautare produs: <nume_produs>\n");
        freeCommandParams(&params);
        return result;
    }

    //strcpy(result, "Comanda <cautare produs> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result, params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result, params.params[i]);
        strcat(result, "\n");
    }

    freeCommandParams(&params);
    return result;
}

char* vizualizare_sold(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    
    strcpy(result, "Comanda <vizualizare sold> a fost receptionata!");

    return result;
}

char* modificare_sold(char *cmd){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 2){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "modificare sold: <adaugare/retragere> <suma>\n");
        freeCommandParams(&params);
        return result;
    }
    
    //strcpy(result, "Comanda <modificare sold> a fost receptionata!");
    strcat(result, "S-a receptionat comanda: ");
    strcat(result,params.command);
    strcat(result, "\n Cu urmatorii parametrii: \n");
    for(int i=0; i<params.numParams; i++){
        strcat(result,params.params[i]);
        strcat(result, "\n");
    }

    return result;
}

char* logout_client(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if(result == NULL){
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        return result;
    }
    else{
        sqlite3_stmt *stmt_upd;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_upd;

        char *query2 = sqlite3_mprintf("UPDATE Useri SET logged = 0 WHERE id = '%d';", *id);
        res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
        sqlite3_step(stmt_upd);
        sqlite3_finalize(stmt_upd);
        strcat(result, "V-ati delogat cu succes!\n");
        sqlite3_close(db);
    }
    *id = -1;
    //strcpy(result, "Comanda <logout> a fost receptionata!");

    return result;
}

char* help_cmd(){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    
    strcpy(result, "Comenzile disponibile sunt:\n");
    strcat(result, "1) login: <username> <password>\n");
    strcat(result, "2) creare cont: <username> <password> <sold>\n");
    strcat(result, "3) lista produse\n");
    strcat(result, "4) creare oferta: <nume_produs> <pret_produs>\n");
    strcat(result, "5) modificare oferta: <id> <nume_produs> <pret_produs>\n");
    strcat(result, "6) stergere oferta: <id>\n");
    strcat(result, "7) istoric achizitii\n");
    strcat(result, "8) cumparare produs: <id>\n");
    strcat(result, "9) cautare produs: <nume_produs>\n");
    strcat(result, "10) vizualizare sold\n");
    strcat(result, "11) modificare sold: <adaugare/retragere> <suma>\n");
    strcat(result, "12) logout\n");
    strcat(result, "13) exit\n");

    return result;
}

char* invalid_command(){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    strcpy(result, "Comanda este invalida!");

    return result;
}

char* manager_comenzi(char *comanda, int *id)
{
    if (strstr(comanda, "login"))
    {
        return login_client(comanda, id);
    }
    else if (strstr(comanda, "creare cont"))
    {
        return creare_cont(comanda);
    }
    else if (strstr(comanda, "lista produse"))
    {
        return lista_produse(comanda);
    }
    else if (strstr(comanda, "creare oferta"))
    {
        return creare_oferta(comanda);
    }
    else if (strstr(comanda, "modificare oferta"))
    {
        return modificare_oferta(comanda);
    }
    else if (strstr(comanda, "stergere oferta"))
    {
        return stergere_oferta(comanda);
    }
    else if (strstr(comanda, "istoric achizitii"))
    {
        return istoric_achizitii(comanda);
    }
    else if (strstr(comanda, "cumparare produs"))
    {
        return cumparare_produs(comanda);
    }
    else if (strstr(comanda, "cautare produs"))
    {
        return cautare_produs(comanda);
    }
    else if (strstr(comanda, "vizualizare sold"))
    {
        return vizualizare_sold(comanda);
    }
    else if (strstr(comanda, "modificare sold"))
    {
        return modificare_sold(comanda);
    }
    else if (strstr(comanda, "help")){
        return help_cmd();
    }
    else if(strstr(comanda, "logout")){
        return logout_client(comanda, id);
    }
    else if (strstr(comanda, "exit"))
    {
        return exit_f(id);
    }
    else
    {
        return invalid_command();
    }
}

int main ()
{
  struct sockaddr_in server;	// structura folosita de server
  struct sockaddr_in from;	
  int nr;		//mesajul primit de trimis la client 
  int sd;		//descriptorul de socket 
  int pid;
  pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
  int i=0;
  

  /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  
  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
  /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }
  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
    {
      int client;
      thData * td; //parametru functia executata de thread     
      int length = sizeof (from);

      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);

      // client= malloc(sizeof(int));
      /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
      if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
	{
	  perror ("[server]Eroare la accept().\n");
	  continue;
	}
	
        /* s-a realizat conexiunea, se astepta mesajul */
    
	// int idThread; //id-ul threadului
	// int cl; //descriptorul intors de accept

	td=(struct thData*)malloc(sizeof(struct thData));	
	td->idThread=i++;
	td->cl=client;

	pthread_create(&th[i], NULL, &treat, td);	      
				
	}//while    
};				
static void *treat(void * arg)
{		
		struct thData tdL; 
		tdL= *((struct thData*)arg);	
		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
		pthread_detach(pthread_self());		
		raspunde((struct thData*)arg);
		/* am terminat cu acest client, inchidem conexiunea */
		close ((intptr_t)arg);
		return(NULL);	
  		
};


void raspunde(void *arg)
{
    int id = -1;
    char nr[MAX_COMMAND_LENGTH];
    //int nr, 
    int i=0;
	struct thData tdL; 
	tdL= *((struct thData*)arg);
    while (1)
    {
        fflush(stdout);
        bzero(nr, sizeof(nr));
        int bytes_read;
        if ((bytes_read = read (tdL.cl, &nr, sizeof(nr))) <= 0)
        {
          if(bytes_read == 0){
            printf("[Thread %d] Clientul s-a deconectat.\n", tdL.idThread);
          }
          else{
            printf("[Thread %d]\n",tdL.idThread);
            perror ("Eroare la read() de la client.\n");    
          }
          break;
        }
        
        printf ("[Thread %d]Mesajul a fost receptionat...%s\n",tdL.idThread, nr);
                char *m;
                //memset(m,0,50);
                m = manager_comenzi(nr, &id);
                /*pregatim mesajul de raspuns */
                //nr++;      
        printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, m);
                
                
                /* returnam mesajul clientului */
        if (write (tdL.cl, m, strlen(m + 1) +1) <= 0)
        {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }
        free(m);
        fflush(stdout);
        
    }
}
