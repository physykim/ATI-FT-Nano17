/*
File    : read_data_single.c
Author  : Sooyeon Kim
Date    : March 06, 2024
Update  : March 28, 2024 -- F/T data calibration
Description : Read force/torque data from Net F/T device
            : Calibration force (Counts per Force:1000000, Counts per Torque:1000000)
Protocol    :
*/

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h> // For _kbhit and _getch

#define PORT 49152 /* Port the Net F/T always uses */
#define COMMAND 2 /* Command code 2 starts streaming */
#define NUM_SAMPLES 1 /* Will send 1 sample before stopping */

/* Typedefs used so integer sizes are more explicit */
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char byte;
typedef struct response_struct {
    uint32 rdt_sequence;
    uint32 ft_sequence;
    uint32 status;
    int32 ft_data[6];
} response;

int main() {
    int socket_handle;            /* Handle to UDP socket used to communicate with Net F/T. */
    struct sockaddr_in addr;    /* Address of Net F/T. */
    byte request[8];            /* The request data sent to the Net F/T. */
    response resp;                /* The structured response received from the Net F/T. */
    byte response[36];            /* The raw response data received from the Net F/T. */
    int i;                        /* Generic loop/array index. */
    int err;                    /* Error status of operations. */
    char* axes[] = { "Fx", "Fy", "Fz", "Tx", "Ty", "Tz" };    /* The names of the force and torque axes. */

    WSADATA wsa_data;
    int ws_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ws_result != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", ws_result);
        return -1;
    }

    socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_handle == -1) {
        fprintf(stderr, "Error creating socket\n");
        WSACleanup();
        return -1;
    }


    // Hardcoded IP address here
    const char* ip_address = "192.168.1.1";

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, ip_address, &addr.sin_addr);
    inet_pton(AF_INET, ip_address, &addr.sin_addr);

    err = connect(socket_handle, (struct sockaddr*)&addr, sizeof(addr));
    if (err == -1) {
        fprintf(stderr, "Error connecting socket\n");
        closesocket(socket_handle);
        WSACleanup();
        return -1;
    }

    printf("Connected to Net F/T at %s\n", ip_address);


    *(uint16*)&request[0] = htons(0x1234); /* standard header. */
    *(uint16*)&request[2] = htons(COMMAND); /* per table 9.1 in Net F/T user manual. */
    *(uint32*)&request[4] = htonl(NUM_SAMPLES); /* see section 9.1 in Net F/T user manual. */

    /* Sending the request. */
    send(socket_handle, request, 8, 0);

    printf("Waiting for data...\n");

    while (!_kbhit()) { // Loop until ESC key is pressed
        /* Receiving the response. */
        int bytes_received = recv(socket_handle, response, 36, 0);
        if (bytes_received == -1) {
            fprintf(stderr, "Error receiving data\n");
            break;
        }

        resp.rdt_sequence = ntohl(*(uint32*)&response[0]);
        resp.ft_sequence = ntohl(*(uint32*)&response[4]);
        resp.status = ntohl(*(uint32*)&response[8]);
        for (i = 0; i < 6; i++) {
            resp.ft_data[i] = ntohl(*(int32*)&response[12 + i * 4]);
        }

        /* Output the response data. */
        printf("\nReceived data:\n");
        printf("RDT Sequence: %u\n", resp.rdt_sequence);
        printf("FT Sequence: %u\n", resp.ft_sequence);
        printf("Status: 0x%08x\n", resp.status);
        for (i = 0; i < 6; i++) {
            printf("%s: %d\n", axes[i], resp.ft_data[i]);
        }
        break;
    }

    closesocket(socket_handle);
    WSACleanup();

    return 0;
}
