/*
File    : clientDataTx.cpp
Author  : Sooyeon Kim
Date    : March 06, 2024
Update  : March 27, 2024 -- TCP/IP client connect to server
Description : This code represents a client that reads force/torque data from a Net F/T device
            : and sends it to another server. Calibration force (Counts per Force:1000000, Counts per Torque:1000000)
						: C++98
Protocol    :
*/

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h> // For _kbhit and _getch
#include <stdint.h>

#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")


#define PORT 4578
#define SERVER_IP "192.168.0.140"

#define PORT_FT 49152 /* Port the Net F/T always uses */
#define COMMAND 2 /* Command code 2 starts streaming */
#define NUM_SAMPLES 0 /* Will continuously stream */
#define SERVER_FT_IP "192.168.1.1"

/* Typedefs used so integer sizes are more explicit */
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char byte;
typedef struct response_struct {
    uint32 rdt_sequence;/* The position of the RDT record within a single output stream. */
    uint32 ft_sequence; /* The internal sample number of the F/T record contained in this RDT record */
    uint32 status;      /* Contains the system status code at the time of the record. */
    int32 FTData[6];    /* The F/T data as counts values */
} RESPONSE;



int main() {
    byte request[8];            /* The request data sent to the Net F/T. */
    RESPONSE resp;                /* The structured response received from the Net F/T. */
    byte response[36];            /* The raw response data received from the Net F/T. */
    int i;                        /* Generic loop/array index. */
    const char* AXES[] = { "Fx", "Fy", "Fz", "Tx", "Ty", "Tz" };    /* The names of the force and torque axes. */
    int hSocket_FT;

    /*****************************************/
    /**** Initialize TCP/IP communication ****/
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }

    SOCKET hSocket;
    hSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (hSocket == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed for main server.\n");
        closesocket(hSocket);
        WSACleanup();
        return 1;
    }
    hSocket_FT = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (hSocket_FT == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed for Net F/T.\n");
        WSACleanup();
        return 1;
    }

    SOCKADDR_IN serverAddr, serverAddr_FT;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0) {
        fprintf(stderr, "inet_pton failed.\n");
        closesocket(hSocket);
        WSACleanup();
        return 1;
    }
    if (connect(hSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr, "Connection failed for main server.\n");
        closesocket(hSocket);
        WSACleanup();
        return 1;
    }

    printf("Connected to main server at %s\n", SERVER_IP);

    memset(&serverAddr_FT, 0, sizeof(serverAddr_FT));
    serverAddr_FT.sin_family = AF_INET;
    serverAddr_FT.sin_port = htons(PORT_FT);
    inet_pton(AF_INET, SERVER_FT_IP, &serverAddr_FT.sin_addr);
    if (inet_pton(AF_INET, SERVER_FT_IP, &serverAddr_FT.sin_addr) <= 0) {
        fprintf(stderr, "inet_pton failed.\n");
        closesocket(hSocket_FT);
        WSACleanup();
        return 1;
    }
    if (connect(hSocket_FT, (SOCKADDR*)&serverAddr_FT, sizeof(serverAddr_FT)) == SOCKET_ERROR) {
        fprintf(stderr, "Connection failed for Net F/T.\n");
        closesocket(hSocket_FT);
        WSACleanup();
        return 1;
    }

    printf("Connected to Net F/T at %s\n", SERVER_FT_IP);


    *(uint16*)&request[0] = htons(0x1234); /* standard header. */
    *(uint16*)&request[2] = htons(COMMAND); /* per table 9.1 in Net F/T user manual. */
    *(uint32*)&request[4] = htonl(NUM_SAMPLES); /* see section 9.1 in Net F/T user manual. */

    /* Sending the request. */
    send(hSocket_FT, reinterpret_cast<const char*>(request), 8, 0);


    printf("Waiting for data...\n");


    while (1) {
        if (_kbhit()) { // Loop until ESC key is pressed
            int ch = _getch();
            if (ch == 27) {
                printf("Exiting program.\n");
                closesocket(hSocket);
                closesocket(hSocket_FT);
                break;
            }
        }

        /* Receiving the response. */
        int bytes_received = recv(hSocket_FT, reinterpret_cast<char*>(response), 36, 0);
        if (bytes_received == -1) {
            fprintf(stderr, "Error receiving data\n");
            break;
        }

        resp.rdt_sequence = ntohl(*(uint32*)&response[0]);
        resp.ft_sequence = ntohl(*(uint32*)&response[4]);
        resp.status = ntohl(*(uint32*)&response[8]);
        for (i = 0; i < 6; i++) {
            resp.FTData[i] = ntohl(*(int32*)&response[12 + i * 4]);
        }

        /* Output the response data. */
        //printf("\nReceived data:\n");
        //printf("RDT Sequence: %u\n", resp.rdt_sequence);
        //printf("FT Sequence: %u\n", resp.ft_sequence);
        //printf("Status: 0x%08x\n", resp.status);
        //for (i = 0; i < 6; i++) {
        //    printf("%s: %d\n", AXES[i], resp.FTData[i]);
        //}

        printf("%u ::\t", resp.rdt_sequence);
        for (i = 0; i < 6; i++) { printf("%s: %d\t", AXES[i], resp.FTData[i]); }
        printf("\n");


        /* Sending data to main server */
        char buffer[sizeof(RESPONSE)];
        int offset = 0;
        *(uint32*)&buffer[offset] = htonl(resp.rdt_sequence);
        offset += sizeof(uint32);
        *(uint32*)&buffer[offset] = htonl(resp.ft_sequence);
        offset += sizeof(uint32);
        *(uint32*)&buffer[offset] = htonl(resp.status);
        offset += sizeof(uint32);
        for (int i = 0; i < 6; i++) {
            *(int32*)&buffer[offset] = htonl(resp.FTData[i]);
            offset += sizeof(int32);
        }
        send(hSocket, buffer, sizeof(buffer), 0);
        //std::this_thread::sleep_for(std::chrono::milliseconds(50));
        //break;
    }

    
    WSACleanup();

    return 0;
}
