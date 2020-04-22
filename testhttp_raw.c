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

#define BUFFER_SIZE 1000000
#define TABLE_SIZE 1000

/* Funkcja dzieli pierwszy argument na adres + port */
void decomposeFirstArgument(char *connectionAddress, char *port, char *address) {
   int i = 0;
   // dwukropek odziela adres od portu
   while (connectionAddress[i] != ':' && connectionAddress[i] != '\0') {
      address[i] = connectionAddress[i];
      i++;
   }
   if (connectionAddress[i] == '\0') {
      fatal("no port number");
   }
   address[i] = '\0';
   i++;
   int shift = i;
   // dzielimy 'connectionAddress' na adres właściwy i port
   while (connectionAddress[i] != '\0') {
      port[i-shift] = connectionAddress[i];
      i++;
   }
   port[i-shift] = '\0';
}

/* Funkcja parsuje plik z ciasteczkami i konkatenuje je */
void findCookies(char* cookies, char* cookiesFile) {
   bool isThereAnyCookie = false;
   FILE *fp;
   char sign;
   char cookie[TABLE_SIZE];
   int i = 0;

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
         bzero(cookie, TABLE_SIZE);
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
      strcpy(cookies, "\r\n");
   } else {
      cookies[strlen(cookies)-2] = '\r';
      cookies[strlen(cookies)-1] = '\n';
   }
}

/* Funkcja znajduje hosta i ścieżkę w trzecim argumencie programu */
void getHostAndPath(char* host, char* path, char* testAddress) {
   int i = 0, j = 1;
   // najpierw szukamy hosta, który jest po substringu "://"
   int index = search("://", testAddress, 0);
   // czytamy albo do końca stringa, albo do ":" (dalej jest nr portu),
   // albo do "/", wtedy zaczyna się ścieżka do zasobu
   while (testAddress[index] != '/'
   && testAddress[index] != ':'
   && testAddress[index] != '\0') {
      host[i] = testAddress[index];
      i++;
      index++;
   }
   host[i] = '\0';
   // po ":" jest nr portu
   if (testAddress[index] == ':') {
      index = search("/", testAddress, index);
      if (index == -1) return;
   // po "/" jest ścieżka do zasobu
   } else if (testAddress[index] == '/') {
      index++;
   } else {
      return;
   }
   while (testAddress[index] != '\0') {
      path[j] = testAddress[index];
      j++;
      index++;
   }
   path[j] = '\0';
}

/* Funkcja tworzy wiadomość GET HTTP */
void createMessage(char *connectionAddress, char* testAddress, char* message, char *cookiesFile) {
   char cookies[BUFFER_SIZE] = "";
   char* part1 = "GET ";
   char* part2 = " HTTP/1.1\r\nHost: ";
   char* part3 = "Connection: close\r\n\r\n";
   char path[TABLE_SIZE] = "/";
   char host[TABLE_SIZE] = "";

   getHostAndPath(host, path, testAddress);
   //findPath(path, testAddress);
   findCookies(cookies, cookiesFile);

   // łączymy poszczególne części wiadomości
   strcat(message, part1);
   strcat(message, path);
   strcat(message, part2);
   strcat(message, host);
   strcat(message, cookies);
   strcat(message, part3);
}

/* Funkcja sprawdza status code otrzymanej odpowiedzi */
void checkStatusCode(char *buffer, int messageLength, char* statusCode) {
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
}

/* Funkcja obsługuje odpowiedź z kodem różnym od 200 */
void serveCodeNot200(char *buffer) {
   char firstLine[TABLE_SIZE];
   int i = 0;
   while (buffer[i] != '\n') {
      firstLine[i] = buffer[i];
      i++;
   }
   firstLine[i] = '\0';
   // drukujemy pierwszy wiersz odpowedzi jako raport
   printf(firstLine);
   printf("\n");
}

/* Funkcja szuka wzorca *pat w stringu *txt od pozycji startingPosition */
int search(char* pat, char* txt, int startingPosition) {
   int M = strlen(pat);
   int N = strlen(txt);

   for (int i = startingPosition; i <= N - M; i++) {
      int j;
      for (j = 0; j < M; j++) {
         if (txt[i + j] != pat[j]) {
            break;
         }
         if (j == (M-1)) {
            return (i+j+1);   // pierwszy indeks po wystąpieniu wzorca
         }
      }
   }
   return -1;
}

/* Funkcja sprawdza czy jest nagłówek "Transfer-Encoding: chunked" */
bool isEncodingChunked(char *buffer, int messageLength) {
   int index = search("Transfer-Encoding: chunked", buffer, 0);
   if (index == -1) {
      return false;
   }
   return true;
}

/* jeśli nie-chunked to funkcja szuka nagłówka Content-Length i ściąga liczbę za nim */
int findContentLength(char *buffer) {
   int index = search("Content-Length: ", buffer, 0);
   if (index == -1) {
      return -1;
   }
   char ch_length[TABLE_SIZE];
   char c = buffer[index];
   int i = 0;
   // czytamy liczbę do znaku '\r'
   while (c != '\r') {
      ch_length[i] = c;
      i++;
      index++;
      c = buffer[index];
   }
   ch_length[i] = '\0';
   return atoi(ch_length);
}

/* Funkcja parsuje bufor i szuka ciasteczek */
void getCookiesFromResponse(char* buffer, char* cookies) {
   char cookie[TABLE_SIZE];
   bzero(cookie, TABLE_SIZE);
   // szukamy pierwszego nagłówka "Set-Cookie"
   int index = search("Set-Cookie: ", buffer, 0);
   int i = 0;
   char c;
   // jeśli index == -1 tzn, że nie ma już więcej nagłówków
   while (index != -1) {
      c = buffer[index];
      while (c != ';' && c != '\r') {
         cookie[i] = c;
         i++;
         index++;
         c = buffer[index];
      }
      cookie[i] = '\0';
      // dołączamy ciasteczko do pozostałych
      strcat(cookies, cookie);
      strcat(cookies, "\n");
      bzero(cookie, TABLE_SIZE);
      i = 0;
      // szukamy kolejnego nagłówka
      index = search("Set-Cookie: ", buffer, index);
   }
}

/* Funkcja sprawdza czy kandydat na długość chunka faktycznie nim jest */
bool isChunkLength(char* buffer, int *index, char *chunk_length) {
   // TO DO
   int N = strlen(buffer);
   bool isChunkLen = false;
   int i = 0;

   // długość chunka zawiera tylko cyfry z systemu hex
   while (buffer[*index] != '\r' && *index < N) {
      if ((buffer[*index] >= '0' &&  buffer[*index] <= '9')
      || (buffer[*index] >= 'a' &&  buffer[*index] <= 'f')) {
         // jest ok, to cyfra hex
         chunk_length[i] = buffer[*index];
         i++;
         (*index)++;
      } else {
         return false;
      }
   }
   chunk_length[i] = '\0';
   return true;
}

/* Funkcja szuka długości chunków w buforze */
int addChunks(char* buffer) {
   int sumOfChunks = 0;
   char chunk_length[BUFFER_SIZE];
   bzero(chunk_length, BUFFER_SIZE);
   int N = strlen(buffer);
   int index = search("\r\n", buffer, 0);
   while (index != -1 && index < N) {
      if (isChunkLength(buffer, &index, chunk_length)) {
         // w chunk_length jest długość kolejnego chunka
         sumOfChunks += strtol(chunk_length, NULL, 16);
         // przesuwamy się o długość chunka
         index += strtol(chunk_length, NULL, 16);
      }
      bzero(chunk_length, BUFFER_SIZE);
      index = search("\r\n", buffer, index);
   }

   return sumOfChunks;
}

/* Funkcja tworzy gniazdo (socket) i nawiązuje połączenie z serwerem */
int socket_connect(char *host, char* port){
   int sock, err;
   struct addrinfo addr_hints;
   struct addrinfo *addr_result;

   memset(&addr_hints, 0, sizeof(struct addrinfo));
   addr_hints.ai_family = AF_INET; // IPv4
   addr_hints.ai_socktype = SOCK_STREAM;
   addr_hints.ai_protocol = IPPROTO_TCP;
   err = getaddrinfo(host, port, &addr_hints, &addr_result);
   if (err == EAI_SYSTEM) { // system error
     syserr("getaddrinfo: %s", gai_strerror(err));
   }
   else if (err != 0) { // other error (host not found, etc.)
     fatal("getaddrinfo: %s", gai_strerror(err));
   }

   sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
   if (sock < 0)
     syserr("socket");

   // connect socket to the server
   if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
     syserr("connect");

   freeaddrinfo(addr_result);
   return sock;
}

bool checkIfHeadersRead(char* buffer) {
   int index = search("\r\n\r\n", buffer, 0);
   if (index == -1) {
      return false;
   }
   return true;
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
   char port[TABLE_SIZE];
   char address[TABLE_SIZE];
   decomposeFirstArgument(connectionAddress, port, address);

   // tworzymy połączenie
   fd = socket_connect(address, port);
   char message[BUFFER_SIZE];
   // tworzymy wiadomość http get
   createMessage(address, testAddress, message, cookiesFile);
   // wysyłamy wiadomość
   int messageLength = strlen(message);
   if (write(fd, message, messageLength) != messageLength) {
      syserr("write");
   }
   bzero(buffer, BUFFER_SIZE);
   int numberOfReads = 0;
   int lengthOfResource = 0;
   bool isChunked = false;
   int contentLength = 0;
   char cookies[BUFFER_SIZE] = "";
   bzero(cookies, BUFFER_SIZE);
   char statusCode[4];

   int curPosition = 0;
   bool headersRead = false;

   // czytamy aż trafimy na nagłówek
   while ((messageLength = read(fd, (buffer+curPosition), BUFFER_SIZE - 1)) != 0
   && !headersRead) {
      if (messageLength < 0) {
         syserr("read");
      }
      headersRead = checkIfHeadersRead(buffer);
      curPosition += messageLength;
   }

   // sprawdzamy nagłówek
   checkStatusCode(buffer, messageLength, statusCode);
   if (atoi(statusCode) != 200) {
      serveCodeNot200(buffer);
   } else {
      isChunked = isEncodingChunked(buffer, messageLength);
      if (!isChunked) {
         contentLength = findContentLength(buffer);
      } else {
         contentLength += addChunks(buffer);
      }
      getCookiesFromResponse(buffer, cookies);
   }

   bzero(buffer, BUFFER_SIZE);

   // czytamy do końca
   while (messageLength = read(fd, buffer, BUFFER_SIZE - 1)) {
      if (messageLength < 0) {
         syserr("read");
      }
      if (isChunked) {
         contentLength += addChunks(buffer);
      }
      bzero(buffer, BUFFER_SIZE);
   }

   // dla kodu 200 drukujemy raport
   if (atoi(statusCode) == 200) {
      printf(cookies);
      printf("%d\n", contentLength);
   }

   // kończymy połaczenie
   shutdown(fd, SHUT_RDWR);
   close(fd);

   return 0;
}
