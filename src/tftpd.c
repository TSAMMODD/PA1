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

int parseOpCode(char* message) {
    return message[1];
}

void parseFileName(char* message, char* fileName) {
    strcpy(fileName, message + 2);
}

void parseFileMode(char* message, char* fileMode, int fileNameSize) {
    strcpy(fileMode, message + 2 + fileNameSize + 1);
}

void parseFileContent(char* directory, char* fileName) {
    FILE *fp;
    char path[512];
    char ch;

    strcpy(path, directory);
    strcat(path, "/");
    strcat(path, fileName);    
    fp = fopen(path, "r");

    if(fp == NULL) {
        perror("Error while opening the file.\n");
        exit(EXIT_FAILURE);
    }

    while((ch = fgetc(fp)) != EOF ) {
        printf("%c",ch);
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
            /* Receive one byte less than declared,
               because it will be zero-termianted
               below. */
            ssize_t n = recvfrom(sockfd, message,
                    sizeof(message) - 1, 0,
                    (struct sockaddr *) &client,
                    &len);
            //
            int opCode;
            char* directory = NULL;
            char fileName[512];
            char fileMode[512];

            directory = argv[2];
            opCode = parseOpCode(message);
            parseFileName(message, fileName);
            parseFileMode(message, fileMode, strlen(fileName));
            parseFileContent(directory, fileName);

            fprintf(stdout, "directory: %s\n", directory);
            fflush(stdout);
            fprintf(stdout, "opCode: %d\n", opCode);
            fflush(stdout);
            fprintf(stdout, "fileName: %s\n", fileName);
            fflush(stdout);
            fprintf(stdout, "fileMode: %s\n", fileMode);
            fflush(stdout);
            //

            /* Send the message back. */
            sendto(sockfd, message, (size_t) n, 0,
                    (struct sockaddr *) &client,
                    (socklen_t) sizeof(client));
            /* Zero terminate the message, otherwise
               printf may access memory outside of the
               string. */
            message[n] = '\0';
            /* Print the message to stdout and flush. */
            fprintf(stdout, "Received:\n%s\n", message);
            fflush(stdout);
        } else {
            fprintf(stdout, "No message in five seconds.\n");
            fflush(stdout);
        }
    }
}
