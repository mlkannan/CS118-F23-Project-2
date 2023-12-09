#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

#define LAST_PKT_SENDS 5

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

    printf("File opened\n");

    int iter = 0;
    while ( 1 )
    {
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr_from, &addr_size);

        if ( buffer.seqnum == expected_seq_num )
        { // GOOD ACK
            expected_seq_num += 1;
            printf("\n%d", recv_len);
            ack_pkt.acknum = expected_seq_num;
            ack_pkt.seqnum = expected_seq_num;
            ack_pkt.last = buffer.last;
            ack_pkt.ack = '1';
            ack_pkt.length = PAYLOAD_SIZE;
            sendto(send_sockfd, &ack_pkt, ack_pkt.length, 0, (struct sockaddr*)&client_addr_to, addr_size);
            printf("\nGood ACK = %d, LAST = %c\n", ack_pkt.acknum, ack_pkt.last);

            fprintf(fp, "%.*s", recv_len, buffer.payload);

            if (ack_pkt.last == '1')
            {
                for (int i = 1; i < LAST_PKT_SENDS; i++ )
                    sendto(send_sockfd, &ack_pkt, ack_pkt.length, 0, (struct sockaddr*)&client_addr_to, addr_size);
                break;
            }
        }
        else if ( buffer.seqnum < expected_seq_num ) // we can ignore this packet; we have it already
        {
            ack_pkt.acknum = buffer.seqnum + 1;
            ack_pkt.seqnum = buffer.seqnum;
            ack_pkt.ack = '1';
            ack_pkt.last = '0';
            ack_pkt.length = PAYLOAD_SIZE;
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt.length), 0, (struct sockaddr*)&client_addr_to, addr_size);
            printf("\nDuplicate Packet with ACK = %d\n", ack_pkt.acknum);
        }
        else
        {
            ack_pkt.acknum = expected_seq_num;
            ack_pkt.seqnum = expected_seq_num;
            ack_pkt.ack = '0';
            ack_pkt.last = '0';
            ack_pkt.length = PAYLOAD_SIZE;
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt.length), 0, (struct sockaddr*)&client_addr_to, addr_size);
            printf("\nPacket ACK'd bad with ACK = %d\n", ack_pkt.acknum);
        }
    }

    /*
    while((bytes_read = recvfrom(listen_sockfd, output_buffer, sizeof(output_buffer) - 1, 0, (struct sockaddr*)&client_addr_from, &addr_size)) > 0)
    {
        fprintf(fp, "%.*s", sizeof(output_buffer), output_buffer);
        printf("%s", buffer);
        bzero(output_buffer, sizeof(output_buffer));
    }
    */
    
    printf("Done receiving");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
