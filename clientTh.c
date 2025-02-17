#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h> 

/* codul de eroare returnat de anumite apeluri */
extern int errno;
#define MAX_COMMAND_LENGTH 1028

/* portul de conectare la server*/
int port;

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char raspuns[MAX_COMMAND_LENGTH];
  char buf[MAX_COMMAND_LENGTH];

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }

  int iesire = 1;
  printf("==== LocalMarketplacePlatform ====\n");
  printf("Tastati comanda <help> pentru a vedea comenzile disponibile! \n");
  while(1){
  /* citirea mesajului */
  printf ("[client]Introduceti o comanda: ");
  fflush (stdout);
  bzero(buf, MAX_COMMAND_LENGTH);
  read (0, buf, sizeof(buf));

  //printf("[client] Am citit %s\n",buf);
  

  /* trimiterea mesajului la server */
  if (write (sd,&buf,sizeof(buf)) <= 0)
    {
      perror ("[client]Eroare la write() spre server.\n");
      return errno;
    }
  else{
    do{
        bzero(raspuns, MAX_COMMAND_LENGTH);
        /* citirea raspunsului dat de server 
            (apel blocant pina cind serverul raspunde) */
        if (read (sd, &raspuns, sizeof(raspuns)) < 0)
        {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }
        fflush(stdout);
        /* afisam mesajul primit */
        printf ("\n%s\n", raspuns);  // mesajul primit de la server
        if(strcmp(raspuns, "exit") == 0){
            exit(1);
        }
        iesire = -1;
        /* inchidem conexiunea, am terminat */
    }while(iesire > 0);
    }
  }
  close (sd);
}
