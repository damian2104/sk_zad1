#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "err.h"

#define BUFFER_SIZE 100000

/* Funkcja wyciąga nr portu z pierwszego argumentu */
void findPortNumber(char *argument, char *port) {
   int i = 0;
   while (argument[i] != ':') {
      i++;
   }
   i++;
   memcpy(port, argument+i, sizeof(char) * (strlen(argument) - i + 1));
   return;
}

int main(int argc, char *argv[]) {
   int sock, err;
   struct addrinfo addr_hints;
   struct addrinfo *addr_result;

   // parsowanie parametrów
   if (argc != 4) {
      fatal("improper nummber of arguments");
   }

   char *connectionAddress = argv[1];   // pierwszy argument
   char *cookiesFile = argv[2];          // drugi argument
   char *testAddress = argv[3];         // trzeci argument
   char port[BUFFER_SIZE];
   findPortNumber(connectionAddress, port);

   // zapisanie danych z parametrów w struct addrinfo
   memset(&addr_hints, 0, sizeof(struct addrinfo));
   addr_hints.ai_family = AF_INET; // IPv4
   addr_hints.ai_socktype = SOCK_STREAM;
   addr_hints.ai_protocol = IPPROTO_TCP;
   err = getaddrinfo(connectionAddress, port, &addr_hints, &addr_result);
   if (err == EAI_SYSTEM) { // systemowy error
     syserr("getaddrinfo: %s", gai_strerror(err));
   }
   else if (err != 0) { // inny błąd
     fatal("getaddrinfo: %s", gai_strerror(err));
   }

   // inicjalizacja socket
   sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
   if (sock < 0) {
      syserr("socket");
   }

   // nawiązanie połączenia z serwerem
   if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0) {
      syserr("connect");
   }

   return 0;
}
