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
    char last = 0;
    char ack = 0;

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


    // Read the entire file and create packets
    size_t bytesRead;
    size_t totalPackets = 0;
    int congestionWindow = 1;
    int numOfRetransmit;

    // Build the packet once
    bytesRead = fread(pkt.payload, 1, sizeof(pkt.payload), fp);
    build_packet(&pkt, totalPackets, 0, 0, 0, bytesRead, pkt.payload);


    while (totalPackets > 0) {

        for (int i = 0; i < congestionWindow && i < totalPackets; ++i) {
            sendto(send_sockfd, &pkt, bytesRead, 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        }

        // Wait for acknowledgment with retry logic
        numOfRetransmit = 0;
/*         while (!receiveAck()) {
            numOfRetransmit++;
        } */

         totalPackets -= congestionWindow;

        if(numOfRetransmit == 0)
        {
            //no loss
            congestionWindow += 1;
        }
        else 
        {
            //loss
            congestionWindow = congestionWindow/2;
        }
    }
    // const size_t PACKET_SIZE = 1024;  // cannot exceed len(data) of 1200
    // char packet[PACKET_SIZE];
    // size_t bytesRead;

    // while ((bytesRead = fread(packet, 1, PACKET_SIZE, fp)) > 0) {
    //     // Process the packet
    //     printf("Packet Size: %zu\n", bytesRead);
    //     //fwrite(packet, 1, bytesRead, stdout);
    //     printf("\n\n");
    // }

    // printf("done");


    // Test basic input/output

    const char test_output[] = "this is a test";
    sendto(send_sockfd, (const char *)test_output, sizeof(test_output), 0, (struct sockaddr*)&server_addr_to, sizeof(struct sockaddr));

    printf("i sent it");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

