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

/* A method that returns the opcode associated
 * with a packet.
 * The opcode is retrieved from the second byte in
 * the packet as the first is a nullbyte.
 *   Opcodes are as follows:
 *  opcode  operation
 *    1     Read request (RRQ)
 *    2     Write request (WRQ)
 *    3     Data (DATA)
 *    4     Acknowledgment (ACK)
 *    5     Error (ERROR)
 */
int parseOpCode(unsigned char* message) {
    return message[1];
}

/* A method that returns the block number associated
 * with a DATA/ACK packet. Block numbers are found in the 
 * third and fourth byte of a packet.
 */
unsigned short parseBlockNumber(unsigned char* message) {
    unsigned short block = message[2] << 8;
    block |= message[3];
    return block;
}

/* A method that accepts as input two strings (char arrays)
 * that represent a RRQ/WRQ packet and the file name 
 * respectively. It copies the filename from the packet
 * which starts at byte three and ends at the next nullbyte. 
 */
void parseFileName(unsigned char* message, unsigned char* fileName) {
    strcpy((char*)fileName, (char*)message + 2);
}

/* A method that accepts as input three strings (char arrays)
 * that represent a RRQ/WRQ packet, the file mode and the size of
 * the file name respectively. It copies the mode field from the
 * packet which starts at the byte that comes after the opcode, filename
 * and the nullbyte after the filename.
 */
void parseFileMode(unsigned char* message, unsigned char* fileMode, int fileNameSize) {
    strcpy((char*)fileMode, (char*)message + 2 + fileNameSize + 1);
}

/* */
void parseFileContent(unsigned char* directory, unsigned char* fileName, int sockfd, struct sockaddr_in client, socklen_t len) {
    FILE *fp;
    unsigned char sendPackage[PACKAGE_LENGTH];
    unsigned char recievePackage[PACKAGE_LENGTH];
    char path[DATA_LENGTH];
    size_t readSize = 0;
    unsigned short blockNumber = 1;

    strcpy(path, (char*)directory);
    strcat(path, "/");
    strcat(path, (char*)fileName);    
    fp = fopen(path, "r");

    if(fp == NULL) {
        perror("Error while opening the file.\n");
        exit(EXIT_FAILURE);
    }

    memset(sendPackage, 0, PACKAGE_LENGTH);
    
    while(!feof(fp)) {
        readSize = fread(&(sendPackage[4]), 1, DATA_LENGTH, fp);
        //fprintf(stdout, "READSIZE: %zu \n", readSize);
        //fflush(stdout);

        sendPackage[1] = OPC_DATA;
        sendPackage[3] = blockNumber & 0xff;
        sendPackage[2] = (blockNumber >> 8) & 0xff;
        sendto(sockfd, sendPackage, readSize + 4, 0, (struct sockaddr *) &client, (socklen_t) sizeof(client));
        memset(sendPackage, 0, PACKAGE_LENGTH);
        recvfrom(sockfd, recievePackage, sizeof(recievePackage), 0, (struct sockaddr *) &client, &len);
        
        // CHECK USER!! => TID of user => port number for example => check better
        if(parseOpCode(recievePackage) != OPC_ACK || parseBlockNumber(recievePackage) != blockNumber) {
            fprintf(stdout, "ERROR! OpCode!! \n");
            fflush(stdout);
            //exit(0);
        }

        memset(recievePackage, 0, PACKAGE_LENGTH);
        blockNumber++;
    }

    fclose(fp);
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in server, client;
    unsigned char message[DATA_LENGTH];

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

            unsigned char fileName[DATA_LENGTH];
            unsigned char fileMode[DATA_LENGTH];
            unsigned char* directory = (unsigned char*) argv[2];
            
            if(parseOpCode(message) == OPC_RRQ) {
                parseFileName(message, fileName);
                parseFileMode(message, fileMode, strlen((char*)fileName));
                parseFileContent(directory, fileName, sockfd, client, len);
            } else {
                // error!
            }
        } else {
            fprintf(stdout, "No message in five seconds.\n");
            fflush(stdout);
        }
    }
}
