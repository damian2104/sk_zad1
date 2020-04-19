#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include "err.h"

#define BUFFER_SIZE 100000

/* Funkcja dzieli pierwszy argument na adres + port */
void decomposeFirstArgument(char *connectionAddress, char **port) {
   int i = 0;
   // dwukropek odziela adres od portu
   while (connectionAddress[i] != ':') {
      i++;
   }
   connectionAddress[i] = '\0';
   // dzielimy 'connectionAddress' na adres właściwy i port
   *port = connectionAddress + i + 1;
   return;
}

/* Funkcja parsuje plik z ciasteczkami i konkatenuje je */
void findCookies(char* cookies, char* cookiesFile) {
   bool isThereAnyCookie = false;
   FILE *fp;
   char sign;
   char cookie[BUFFER_SIZE/1000];
   int i =  0;

   strcat(cookies, "\r\nCookie: ");
   // otwieramy plik z ciasteczkami
   if ((fp = fopen(cookiesFile, "r")) == NULL) {
      fatal("opening cookie file");
   }

   while (true) {
      // czytamy plik znak po znaku
      sign = getc(fp);
      // jeśli znak końca linii, dodajemy ciasteczko do 'cookies'
      if (sign == '\n') {
         cookie[i] = ';';
         cookie[i+1] = ' ';
         strcat(cookies, cookie);
         bzero(cookie, BUFFER_SIZE/1000);
         i = 0;
         isThereAnyCookie = true;
      // jeśli znak EOF kończymy parsowanie
      } else if (sign == EOF) {
         if (cookie[0] == '\0') {
            break;
         // jeśli przed EOF nie było znaku nowej linii
         // zapisujemy ostatnie ciasteczko
         } else {
            cookie[i] = ';';
            cookie[i+1] = ' ';
            strcat(cookies, cookie);
            break;
         }
      } else {
         // zapisujemy następny znak z ciasteczka
         cookie[i] = sign;
         i++;
      }
   }
   strcat(cookies, "\0");
   // jeśli plik pusty, nie chcemy tworzyć nagłówka "Cookie"
   if (!isThereAnyCookie) {
      cookies = "\r\n";
   } else {
      cookies[strlen(cookies)-2] = '\r';
      cookies[strlen(cookies)-1] = '\n';
   }

   return;
}

/* Funkcja tworzy wiadomość GET HTTP */
void createMessage(char *connectionAddress, char message[], char *cookiesFile) {
   char *part1 = "GET / HTTP/1.1\r\nHost: ";
   char *part2 = "Connection: close\r\n\r\n";
   char cookies[BUFFER_SIZE];
   findCookies(cookies, cookiesFile);

   // łączymy poszczególne części wiadomości
   strcat(message, part1);
   strcat(message, connectionAddress);
   strcat(message, cookies);
   strcat(message, part2);

   return;
}

/* Funkcja sprawdza status code otrzymanej odpowiedzi */
void checkStatusCode(char *buffer, int message_length, char statusCode[]) {
   char sign = buffer[0];
   int j = 0;
   // kod jest bezpośrednio po pierwszej spacji w pierwszym wierszu
   while (sign != ' ') {
      j++;
      sign = buffer[j];
   }
   j++;
   // zapisujemy kod
   for (int i = 0; i <= 2; i++) {
      statusCode[i] = buffer[j];
      j++;
   }
   statusCode[3] = '\0';

   return;
}

/* Funkcja obsługuje odpowiedź z kodem różnym od 200 */
void serveCodeNot200(char *buffer) {
   char firstLine[BUFFER_SIZE/1000];
   int i = 0;
   while (buffer[i] != '\n') {
      firstLine[i] = buffer[i];
      i++;
   }
   firstLine[i] = '\0';
   // drukujemy pierwszy wiersz odpowedzi jako raport
   printf(firstLine);
   printf("\n");
   return;
}

/* Funkcja tworzy gniazdo (socket) i nawiązuje połączenie z serwerem */
int socket_connect(char *host, in_port_t port){
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sock;

   // znajdujemy hosta po nazwie
	if ((hp = gethostbyname(host)) == NULL) {
		syserr("gethostbyname");
	}
   // wypełniamy odpowiednie pola w strukturze
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
   // tworzymy gniazdo
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

   // jeśli sock = -1, coś posżło nie tak, więc kończymy program z kodem 1
	if (sock == -1) {
		syserr("setsockopt");
	}

   // connect() = -1 ---> błąd
	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
		syserr("connect");
	}

	return sock;
}

int main(int argc, char *argv[]){
	int fd;
	char buffer[BUFFER_SIZE];

   // liczba argumentów musi być równa 4 (włącznie z nazwą programu)
   if (argc != 4){
      fatal("improper number of arguments");
	}

   char *connectionAddress = argv[1];   // pierwszy argument
   char *cookiesFile = argv[2];          // drugi argument
   char *testAddress = argv[3];         // trzeci argument
   char *port;
   decomposeFirstArgument(connectionAddress, &port);

   // tworzymy połączenie
   fd = socket_connect(connectionAddress, atoi(port));
   char message[BUFFER_SIZE] = "";
   // tworzymy wiadomość http get
   createMessage(connectionAddress, message, cookiesFile);
   // wysyłamy wiadomość
   int message_length = strlen(message);
   if (write(fd, message, message_length) != message_length) {
      syserr("write");
   }
   bzero(buffer, BUFFER_SIZE);
   int numberOfReads = 0;
   int lengthOfResource = 0;

   // czytamy odpowiedź i drukujemy na stdout
   while (message_length = read(fd, buffer, BUFFER_SIZE - 1) != 0) {
      if (message_length < 0) {
         syserr("read");
      }
      numberOfReads++;
      if (numberOfReads == 1) {
         char statusCode[4];
         checkStatusCode(buffer, message_length, statusCode);
         if (atoi(statusCode) != 200) {
            serveCodeNot200(buffer);
            break;
         } else {
            // TO DO
         }
      }
      printf("%s", buffer);
      bzero(buffer, BUFFER_SIZE);
   }

   // kończymy połaczenie
   shutdown(fd, SHUT_RDWR);
   close(fd);
   
   return 0;
}
