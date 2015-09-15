/* A UDP echo server with timeouts.
 *
 * Note that you will not need to use select and the timeout for a
 * tftp server. However, select is also useful if you want to receive
 * from multiple sockets at the same time. Read the documentation for
 * select on how to do this (Hint: Iterate with FD_ISSET()).
 */

#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Constants. */
/* Known lenghts. */
#define PACKAGE_LENGTH 516
#define DATA_LENGTH 512
/* Opcodes. */
#define OPC_RRQ 1
#define OPC_WRQ 2
#define OPC_DATA 3
#define OPC_ACK 4
#define OPC_ERROR 5
/* */

/* */
int parseOpCode(char* message) {
    return message[1];
}

/* */
int parseBlockNumber(char* message) {
    char* tmp = message;
    return (tmp[2] << 8) + tmp[3];
}

/* */
void parseFileName(char* message, char* fileName) {
    strcpy(fileName, message + 2);
}

/* */
void parseFileMode(char* message, char* fileMode, int fileNameSize) {
    strcpy(fileMode, message + 2 + fileNameSize + 1);
}

/* */
void parseFileContent(char* directory, char* fileName, int sockfd, struct sockaddr_in client, socklen_t len) {
    FILE *fp;
    char sendPackage[PACKAGE_LENGTH];
    char recievePackage[PACKAGE_LENGTH];
    char path[DATA_LENGTH];
    size_t readSize = 0;
    short blockNumber = 1;

    strcpy(path, directory);
    strcat(path, "/");
    strcat(path, fileName);    
    fp = fopen(path, "r");

    if(fp == NULL) {
        perror("Error while opening the file.\n");
        exit(EXIT_FAILURE);
    }

    memset(sendPackage, 0, PACKAGE_LENGTH);

    while(!feof(fp)) {
        readSize = fread(&(sendPackage[4]), 1, DATA_LENGTH, fp);
        fprintf(stdout, "READSIZE: %zu \n", readSize);

        sendPackage[1] = OPC_DATA;
        sendPackage[3] = blockNumber & 0xff;
        sendPackage[2] = (blockNumber >> 8) & 0xff;
        sendto(sockfd, sendPackage, readSize + 4, 0, (struct sockaddr *) &client, (socklen_t) sizeof(client));
        memset(sendPackage, 0, PACKAGE_LENGTH);
        recvfrom(sockfd, recievePackage, sizeof(recievePackage), 0, (struct sockaddr *) &client, &len);
        
        if(parseOpCode(recievePackage) != OPC_ACK || parseBlockNumber(recievePackage) != blockNumber) {
            exit(0);
        }

        memset(recievePackage, 0, PACKAGE_LENGTH);
        blockNumber++;
    }

    fclose(fp);
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in server, client;
    char message[512];

    /* Create and bind a UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    /* Network functions need arguments in network byte order instead of
       host byte order. The macros htonl, htons convert the values, */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(atoi(argv[1]));
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

    for (;;) {
        fd_set rfds;
        struct timeval tv;
        int retval;

        /* Check whether there is data on the socket fd. */
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        /* Wait for five seconds. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        } else if (retval > 0) {
            /* Data is available, receive it. */
            assert(FD_ISSET(sockfd, &rfds));

            /* Copy to len, since recvfrom may change it. */
            socklen_t len = (socklen_t) sizeof(client);
            /* */
            recvfrom(sockfd, message, sizeof(message), 0, (struct sockaddr *) &client, &len);

            char fileName[DATA_LENGTH];
            char fileMode[DATA_LENGTH];
            char* directory = argv[2];
            
            if(parseOpCode(message) == OPC_RRQ) {
                parseFileName(message, fileName);
                parseFileMode(message, fileMode, strlen(fileName));
                parseFileContent(directory, fileName, sockfd, client, len);
            }
        } else {
            fprintf(stdout, "No message in five seconds.\n");
            fflush(stdout);
        }
    }
}
