/*
** server.c -- a stream socket server demo
*/
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold



void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {

	int sockfd, new_fd;
	struct addrinfo hints, *serverInfo, *it;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;


	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if(err = getaddrinfo(NULL, PORT, &hints, &serverInfo) != 0)
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));

	for(it = serverInfo; it != NULL; it = it->ai_next) {
		if(sockfd = socket(it->ai_family, it->ai_socktype, it->ai_protocol) == -1) {
			perror("server: socket");
			continue;
		}

		if(bind(sockfd, it->ai_addr, it->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(serverInfo);

	if(it == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	

	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {
		sin_size = sizeof(their_addr);
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size);
		if(new_fd == -1) {
			perror("accept");
			exit(1);
		}

		char *buff[1024];
		char s[INET6_ADDRSTRLEN];

		inet_ntop(
			their_addr.ss_family, 
            get_in_addr((struct sockaddr *)&their_addr),
			s, 
			sizeof(s)
		);

		if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, "Hello, world!", 13, 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

	return 0;
	
	
}


io mi trovo con la vostra visione che l'ai sia qualcosa da
--- LA PROGETTAZIONE NON SARà RIMPIAZZATA
// BASE 44
// Colloqui di lavoro
	- IA vietata
	- GOOGLE assume gente 

// esistono percorsi fondazionali
// esostono degli strumentui
// la IA non è dalla sua integrazione ontologica un percorso fondamentale
// l'intento e ciò che possiamo produrre partendo dalle nostra fondazione concettuale
// l'intento e cio che supporta la assosciazione empresariale tra io e il cliente
// gli strumenti sono in particolare ponti per perfezionare le fondazioni (tu giochi con la calcolatrice, fai lo script in manim, interroghi la IA, tu giochi con uno strumento in funzione di migliorare le fondamenta)
// |- non aprofondisci l'IA, giocandoci mentre aprofondisci le fondazioni.

// la produzione e un altro strumento (impari facendo)

// la differenza tra l'IA e te e che tu ti puoi interessare per un unica cosa, focalizzare l'intento. La IA al momento no. La IA non puo contribuire
// quando la IA si possa simulare l' osessionare per una cosa, non ci rimarrà niente
// per contribuire ci vuole, imprendimento e fondazione.