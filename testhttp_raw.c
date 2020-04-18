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

#include "err.h"

#define BUFFER_SIZE 1000

/* Funkcja dzieli pierwszy argument na adres + port */
void decomposeFirstArgument(char *connectionAddress, char **port) {
   int i = 0;
   while (connectionAddress[i] != ':') {
      i++;
   }
   connectionAddress[i] = '\0';
   // dzielimy 'connectionAddress' na adres właściwy i port
   *port = connectionAddress + i + 1;
   return;
}

/* Funkcja tworzy wiadomość GET HTTP */
void createMessage(char *connectionAddress, char message[]) {
   char *part1 = "GET / HTTP/1.1\r\nHost: ";
   char *part2 = "\r\nConnection: close\r\n\r\n";

   strcat(message, part1);
   strcat(message, connectionAddress);
   strcat(message, part2);
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
   createMessage(connectionAddress, message);

   // wysyłamy wiadomość
   int message_length = strlen(message);
	if (write(fd, message, message_length) != message_length) {
      syserr("write");
   }
	bzero(buffer, BUFFER_SIZE);

   // czytamy odpowiedź i drukujemy na stdout
	while (message_length = read(fd, buffer, BUFFER_SIZE - 1) != 0) {
      if (message_length < 0) {
         syserr("read");
      }
		printf("%s", buffer);
		bzero(buffer, BUFFER_SIZE);
	}

   // kończymy połaczenie
	shutdown(fd, SHUT_RDWR);
	close(fd);

	return 0;
}
