#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "utils.h"

#define INITIAL_BATCH_SIZE 1
#define MAX_BATCH_SIZE 64
#define AIMD_INCREASE_FACTOR 2
#define AIMD_DECREASE_FACTOR 0.5

#define INITIAL_SSTHRESH 6

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

    // Setup timeout interval
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    int num_unacked = 0;

    if (int n = setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 )
    {
        printf("failed to set timeout interval :(\n");
        printf("%d\n", n);
        return 1;
    }

    int total_packets = ( (filesize + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE ); // 
    // TODO: printf("total packets = %d\n\n", total_packets );

    // int current_batch_size = INITIAL_BATCH_SIZE;
    // int current_batch_size = 8;
    float cwnd = INITIAL_BATCH_SIZE;
    int ssthresh = INITIAL_SSTHRESH;
    int dup_acks = 0;
    int packets_sent = 0;

    bool fast_retransmit = false;

    while (1) {
        printf("\n\nstarting over\n");
        // send out ALL remaining packets in cwnd
        while ( packets_sent < (int)cwnd && seq_num < total_packets )
        {
            if ( seq_num + 1 == total_packets )
            {
                // printf("build the last one\n");
                last = '1';
            }
            // printf("Send packet %d\n", seq_num + packets_sent);
            build_packet(&pkt, seq_num + packets_sent, ack_num, last, ack, PAYLOAD_SIZE, filestart + ((seq_num + packets_sent) * PAYLOAD_SIZE));
            printSend(&pkt, false);
            printf("cwnd = %.6f\n", cwnd);
            sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
            packets_sent++;
        }

        // for (int i = 0; i < cwnd - packets_sent; i++)
        // {
        //     if ( total_packets == seq_num + packets_sent - 1 ) {
        //         last = '1';
        //     } 

        //     build_packet(&pkt, seq_num, cwnd + packets_sent, last, ack, PAYLOAD_SIZE, filestart + (( seq_num + packets_sent ) * PAYLOAD_SIZE));
        //     // printf("Built a packet size: %lu\n", sizeof(pkt.payload));
        //     printSend(&pkt, 0);
        //     ack_num += 1;

        //     sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        //     printf("Finished sending!\n");
        //     usleep(250);
        //     // usleep(250);
        //     packets_sent++;
        //     // seq_num++;

        //     // if (last == '1') {
        //     //     break;
        //     // }
        //     if (last == '1')
        //     {
        //         break;
        //     }
        // }
        if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size) < 0)
        {
            // TIMEOUT
            printf("Timeout while waiting for ACK\n\n");
            last = '0';
            ack_num = seq_num;
            // dup_acks++;

            // FULL RESET
            cwnd = 1; 

            // TODO: WHEN SUBMITTING TO GRADESCOPE USE THIS
            // ssthresh = std::max( (int)(cwnd/2), 2);

            // TODO: WHEN RUNNING LOCALLY, DO THIS
            ssthresh = fmax( (int)(cwnd/2), 2);
            // replace with 
            packets_sent = 0; // reset cwnd buffer
        } 
        else if ( ack_pkt.acknum - 1 == seq_num ) // GOOD ACK
        {
            // SUCCESS
            printRecv(&ack_pkt);
            // printf("Success, ACK = %d\n", ack_num);
            // seq_num = ack_num; // increase SEQ
            // cwnd = AIMD_INCREASE_FACTOR * cwnd;

            if (fast_retransmit)
            {
                cwnd = ssthresh;
                fast_retransmit = false;
            }
            else
            {
                if ((int)cwnd <= ssthresh) {
                    // cwnd += (ack_pkt.acknum - seq_num);
                    cwnd++;
                }
                else {
                    //cwnd += (float)(ack_pkt.acknum - seq_num)/cwnd;
                    cwnd += (1/cwnd);
                    // cwnd += (float)(1/)
                }
                printf("cwnd changed to %.6f\n", cwnd);
            }
            // packets_sent -= (ack_pkt.acknum - seq_num);
            packets_sent = 0;
            seq_num = ack_pkt.acknum;
            ack_num = ack_pkt.acknum;
            dup_acks = 0;

            if ( ack_pkt.last == '1' )
                last = '1';
        } 
        else // BAD ACK
        {
            // // INCORRECT ACK -> ACK < EXP_ACK
            // printf("Bad ACK, ACK = %d\n", ack_num);
            printRecv(&ack_pkt);
            last = '0';
            seq_num = ack_pkt.acknum;
            ack_num = ack_pkt.acknum;
            packets_sent = 0;
            if (dup_acks == 3) // ENTER FAST RETRANSMIT AFTER 3 DUP ACKS
            {
                fast_retransmit = true;

                // TODO: WHEN SUBMITTING TO GRADESCOPE USE THIS
                // ssthresh = std::max( (int)(cwnd/2), 2);

                // TODO: WHEN RUNNING LOCALLY, DO THIS
                ssthresh = fmax( (int)(cwnd/2), 2);
                cwnd = ssthresh + 3;
                build_packet(&pkt, seq_num, ack_num, last, ack, PAYLOAD_SIZE, filestart + (seq_num * PAYLOAD_SIZE));
                sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
            }
            else if (dup_acks > 3)
            {
                //cwnd++;
                cwnd = cwnd/2;
            }
        }

        if (last == '1') {
            // If it's the last packet, break out of the main loop
            break;
        }
        // usleep(250000);
    }

    // printf("DONE TRANSMITTING PACKETS");
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    free(filebuffer);
    return 0;
}