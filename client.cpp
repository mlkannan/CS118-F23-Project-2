#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = '0';
    char ack = '0';

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    // Read whole file into char array buffer
    long filesize;
    fseek( fp , 0L , SEEK_END);
    filesize = ftell( fp );
    rewind( fp );
    printf("Filesize: %ld\n", filesize);

    char* filebuffer = (char*)calloc(1, filesize+1);
    const char* filestart = filebuffer;
    if ( fread( filebuffer, filesize, 1, fp ) <= 0 )
    {
        printf("FAILED TO READ\n");
        return 1;
    }
    printf("Finished reading into filebuffer\n");


    // Create first packet

    unsigned int bytesRead = 0;

    int iter = 0;

    while ( last != '1' )
    {
        build_packet(&pkt, ack_num, seq_num, '0', '0', PAYLOAD_SIZE, filestart + (sizeof(char)*seq_num) );
        printf("Built a packet size: %lu\n", sizeof(pkt.payload));
        if (sizeof(pkt.payload) < PAYLOAD_SIZE)
        {
            last = '1';
            build_packet(&pkt, ack_num, seq_num, '0', '1', PAYLOAD_SIZE, filestart + (sizeof(char)*seq_num) );
        }
        printSend(&pkt, 0);
        ack_num += PAYLOAD_SIZE;

        sendto(send_sockfd, &pkt, pkt.length, 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        printf("Finished sending!\n");

        recvfrom(listen_sockfd, &ack_pkt, sizeof(&ack_pkt), 0, (struct sockaddr*)&server_addr_from, &addr_size);
        printf("Received ACK pkt size: %lu\n", sizeof(ack_pkt.payload));
        if ( ack_pkt.acknum == ack_num )
        {
            printf("Packet successsfully ACK'd with ACK = %d\n", ack_num);
            seq_num = ack_num;
        }
        else
        {
            printf("Failed ACK with ACK = %d\n", ack_pkt.acknum);
            last = '0';
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    free(filebuffer);
    return 0;
}

