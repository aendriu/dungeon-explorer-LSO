#define _POSIX_C_SOURCE 200112L
#include "../header/connection.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

// ************************************************** //

static const char *MSG_MAX_PLAYER_REACHED =
    "The maximum player capacity has been reached!";

static int fetch_public_ip(char *out, size_t out_len) {
    if (out == NULL || out_len == 0)
        return -1;

    FILE *fp = popen("curl -s https://api.ipify.org", "r");
    if (fp == NULL)
        return -1;

    if (fgets(out, (int)out_len, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == '\r' || out[len - 1] == '\n' ||
                       out[len - 1] == ' ' || out[len - 1] == '\t')) {
        out[len - 1] = '\0';
        len--;
    }
    return (len > 0) ? 0 : -1;
}

static void print_ip_info(void) {
    char public_ip[64] = {0};
    if (fetch_public_ip(public_ip, sizeof(public_ip)) == 0) {
        printf("IP pubblico: %s\n", public_ip);
    } else {
        printf("IP pubblico: non disponibile (verifica connessione o NAT)\n");
    }

    printf("Porta: %s\n", PORT);

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) != 0) {
        return;
    }

    printf("IP locali disponibili:\n");
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        char ip[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip)) != NULL) {
            if (strcmp(ip, "127.0.0.1") != 0) {
                printf("- %s (%s)\n", ip, ifa->ifa_name);
            }
        }
    }

    freeifaddrs(ifaddr);
}

int welcome_sock = 0;
bool si_initialized = false;
struct addrinfo *servinfo = NULL;

void init_servinfo() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;     
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    
    int status;
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    if(servinfo == NULL) {
        fprintf(stderr, "init_servinfo: servinfo is NULL\n");
		exit(1);
    }
    si_initialized = true;
    printf("- servinfo initialized\n");
}

// *****

void init_welcome_sock() {
    welcome_sock = socket_create();
    socket_bind(welcome_sock);
    if(listen(welcome_sock, BACKLOG) == -1) {
        perror("[SERVER] - listen in init_welcome_sock()");
        exit(1);
    }
    printf("- welcome socket initialized\n");

    print_ip_info();
}

// *****

void sockets_free(Conn* socks, int size) {
    close(welcome_sock);
    for(int i = 0; i < size; i++) {
        if(socks[i].sockfd != 0)
            close(socks[i].sockfd);
    }
    return;
}

// ************************************************** //

int socket_create() {
    if(!si_initialized) { init_servinfo();}

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
    if( sockfd == -1 ) {
        perror("[SERVER] - socket()");
        exit(1);
    }
    return sockfd;
}

// *****

void socket_bind(int sockfd) {
    int res = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if(res == -1) {
        close(sockfd);
        perror("[SERVER] - bind()");
    }
}

// *****

int listen_loop() {
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof(their_addr);
    int new_sockfd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    return new_sockfd;
}

void listen_loop_refuse() {
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof(their_addr);
    int new_fd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    if (new_fd != -1) {
        char frame[256];
        memset(frame, 0, sizeof(frame));
        strncpy(frame, MSG_MAX_PLAYER_REACHED, sizeof(frame) - 1);
        send(new_fd, frame, sizeof(frame), 0);
        close(new_fd);
    }
}



