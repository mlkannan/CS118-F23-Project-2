#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

#define LAST_PKT_SENDS 5
#define INITIAL_BATCH_SIZE 1
#define MAX_BATCH_SIZE 64
#define AIMD_INCREASE_FACTOR 2
#define AIMD_DECREASE_FACTOR 0.5

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    // int current_batch_size = INITIAL_BATCH_SIZE;
    int current_batch_size = 0;
    int cwnd = 1;
    int last_pkt_seq = -100;
    struct packet packet_buffer[MAX_PACKETS];
    memset(&packet_buffer, 0, sizeof(packet_buffer));
    char last = '0';
    // expected sequence number is equivalent to min sequence number

    printf("File opened\n");

    int packets_received = 0;
    int last_seq_loaded = 0;

    while (1)
    {
        printf("waiting on a packet\n");
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr_from, &addr_size);
        // cwnd = buffer.acknum;

        // If I don't have this packet yet, load it into packet buffer
        printf("recv packet seq = %d\n", buffer.seqnum);
        if ( packet_buffer[buffer.seqnum].length == 0 && recv_len > 0 )
        {
            packet_buffer[buffer.seqnum] = buffer;
            printf("yeha");
            printf("%d\n", packet_buffer[buffer.seqnum].length);
            packets_received++;
        }
        // if ( packets_received == buffer.seqnum + 1 && last_seq_loaded != buffer.seqnum )
        // {
        //     for ( ; last_seq_loaded != buffer.seqnum; last_seq_loaded++ )
        //     {
        //         printf("\n\nLoading packet %d into output\n\n\n", last_seq_loaded);
        //         fprintf(fp, "%.*s", packet_buffer[last_seq_loaded].length, packet_buffer[last_seq_loaded].payload);
        //     }
        // }

        if ( buffer.seqnum == expected_seq_num && packet_buffer[buffer.seqnum].length > 0 ) // else: leave exp_seq unchanged
        {
            expected_seq_num = buffer.seqnum + 1;
            printf("Good ack\n");
        }
        else
        {
            printf("Bad ack\n");
        }

        if ( buffer.last == '1' )
        {
            printf("this is the last one");
        }
        
        ack_pkt.acknum = expected_seq_num;
        ack_pkt.seqnum = expected_seq_num;
        ack_pkt.ack = '1';
        ack_pkt.last = buffer.last;
        ack_pkt.length = PAYLOAD_SIZE;

        sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size);
        printf("sent ack\n");
        printSend(&ack_pkt, false);

        // if ( buffer.last == '1' /*&& last_seq_loaded == expected_seq_num*/ )
        //     break;

        if ( buffer.last == '1' )
        {
            for (int i = 1; i < LAST_PKT_SENDS; i++ )
                sendto(send_sockfd, &ack_pkt, ack_pkt.length, 0, (struct sockaddr*)&client_addr_to, addr_size);
            break;
        }

        // else
        // {
        //     // wait for client side timeout.
        // }
            /*
            if ( buffer.seqnum >= expected_seq_num ) 
            {// GOOD ACK
                // expected_seq_num += 1;
                // fprintf(fp, "%.*s", recv_len, buffer.payload);

                // ack_pkt.acknum = expected_seq_num;
                // ack_pkt.seqnum = expected_seq_num;
                // ack_pkt.ack = '1';
                // ack_pkt.last = buffer.last;
                // ack_pkt.length = PAYLOAD_SIZE;

                // sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr_to, addr_size);
                // printf("\nGood ACK = %d, LAST = %c\n", ack_pkt.acknum, ack_pkt.last);

                if ( packet_buffer[buffer.seqnum - expected_seq_num].length == 0 ) 
                {
                    packet_buffer[buffer.seqnum - expected_seq_num] = buffer;
                    packets_received++;
                    printf("Got packet num%D\n", buffer.seqnum);

                    if ( buffer.last == '1' )
                    {
                        last_pkt_seq = buffer.seqnum;
                        last = '1';
                    }
                }
            } 
            else 
            {
                // Handle duplicate or out-of-order packets
                ack_pkt.acknum = expected_seq_num;
                ack_pkt.seqnum = expected_seq_num;
                ack_pkt.ack = '1';
                ack_pkt.last = '0';
                ack_pkt.length = PAYLOAD_SIZE;
                sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr_to, addr_size);
                printf("\nDuplicate Packet with ACK = %d\n", ack_pkt.acknum);
            }

            // if (buffer.last == '1') {
            //     break;
            // }
            printf("pkt recv = %d, cwnd = %d\n", packets_received, cwnd);
            if ( packets_received == cwnd || packets_received == last_pkt_seq - expected_seq_num ) 
            {
                break;
            }
        }
        for ( int i = 0; i < packets_received; i++ )
        {
            // if ( last == '1' )
            // {
            //     printf("%.s", sizeof(packet_buffer[i].payload), packet_buffer[i].payload);
            //     printf("yeah\n");
            // }
            printf("\n\nLoading %d Packets into output\n\n\n", packets_received);
            fprintf(fp, "%.*s", packet_buffer[i].length, packet_buffer[i].payload);
            usleep(250);
            memset(&packet_buffer[i], 0, sizeof(packet_buffer[i]));
            printf("Buffer cleared\n");
        }

        expected_seq_num += packets_received;
        ack_pkt.acknum = expected_seq_num;
        ack_pkt.seqnum = expected_seq_num;
        ack_pkt.ack = '1';
        ack_pkt.last = last;
        ack_pkt.length = PAYLOAD_SIZE;

        sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr_to, addr_size);
        printf("\nGood ACK = %d, LAST = %c\n", ack_pkt.acknum, ack_pkt.last);

        if ( last == '1' )
        {
            for (int i = 1; i < LAST_PKT_SENDS; i++ )
                sendto(send_sockfd, &ack_pkt, ack_pkt.length, 0, (struct sockaddr*)&client_addr_to, addr_size);
            usleep(500);
            break;
        }
        // if (packets_received == current_batch_size) {
        //     if (current_batch_size * AIMD_INCREASE_FACTOR <= MAX_BATCH_SIZE) {
        //         current_batch_size *= AIMD_INCREASE_FACTOR;
        //     }
        // } 
        // else 
        // {
        //     current_batch_size *= AIMD_DECREASE_FACTOR;
        //     if (current_batch_size < INITIAL_BATCH_SIZE) {
        //         current_batch_size = INITIAL_BATCH_SIZE;
        //     }
        // }

        // if (buffer.last == '1') {
        //     break;
        // }
        */
    }

    for ( int i = 0; i != packets_received; i++ ) // put final packets into output
    {
        // printf("\n\nLoading packet %d into output\n\n\n", i);
        fprintf(fp, "%.*s", packet_buffer[i].length, packet_buffer[i].payload);
    }

    printf("Done receiving");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}