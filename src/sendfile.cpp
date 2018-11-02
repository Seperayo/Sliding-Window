//sendfile.cpp
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <stdio.h>

#include "config.h"

using namespace std;

// Initialize global variables
int sock, windowSize;
unsigned int sockLength, port;
struct sockaddr_in server, from;
struct hostent *hostName;
char *ip;

int lastACKReceived, lastFrameSent;
bool *hasACKReceived;
bool *hasPacketSend;
timeStamp *packetSendTime;

timeStamp TMIN = currentTime();
mutex mutexLock;

// Packet variables
unsigned int sequenceNumber;
unsigned int packetSize, dataSize;
char data[MAX_DATA_LENGTH];
char packet[MAX_PACKET_LENGTH];

// File variables
FILE *file;
char *buffer;
char *fileName;
unsigned int maxBufferSize, bufferSize;

size_t getPacketSize(char* packet, unsigned int sequenceNumber, size_t dataLength, char* data, bool isEOT) {
    
    unsigned int networkSequenceNumber = htonl(sequenceNumber);
    unsigned int networkDataLength = htonl(dataLength);
    
    if (isEOT) {
        packet[0] = 0x0;
    } else {
        packet[0] = 0x1;
    }

    memcpy(packet+1, &networkSequenceNumber, 4);
    memcpy(packet+5, &networkDataLength, 4);
    memcpy(packet+9, data, dataLength);

    packet[9 + dataLength] = countChecksum(dataLength, data);

    return dataLength + (size_t)10;
}

void readACK(char *ACK, bool* isNAK, unsigned int *sequenceNumber, bool *isChecksumValid){
    
    if(ACK[0] == 0x0)
        *isNAK = false;
    else
        *isNAK = true;

    uint32_t networkSequenceNumber;
    memcpy(&networkSequenceNumber, ACK + 1, 4);
    *sequenceNumber = ntohl(networkSequenceNumber);

    *isChecksumValid = ACK[5] == countChecksum((size_t)4, (char *) sequenceNumber);
}

void listenACK() {
    char ACK[ACK_LENGTH];
    unsigned int sequenceNumber;
    bool isChecksumValid, isNAK;

    while(true) {
        int ACKSize = recvfrom(sock, ACK, ACK_LENGTH, MSG_WAITALL,(struct sockaddr *)&from, &sockLength);
        if (ACKSize < 0) {
            cout << "Packet loss detected. Terminating..." << endl;
            exit(1);
        }

        // Create ACK from buffer
        readACK(ACK, &isNAK, &sequenceNumber, &isChecksumValid);

        if (isChecksumValid) {
            mutexLock.lock();
            if (sequenceNumber >= lastACKReceived + 1 && sequenceNumber <= lastFrameSent) {
                if (!isNAK) {
                    hasACKReceived[sequenceNumber - (lastACKReceived + 1)] = true;
                    cout << "ACK " << sequenceNumber << " received." << endl;
                } else {
                    packetSendTime[sequenceNumber - (lastACKReceived + 1)] = TMIN;
                    cout << "NAK " << sequenceNumber << " received." << endl;
                }
            } else {
                cout << "ACK " << sequenceNumber << " out of bound." << endl;
            }
            mutexLock.unlock();
        } else {
            cout << "Corrupt ACK detected." << sequenceNumber << endl;
        }
    }
}

void readArgument(int argc, char *argv[]) {
    if (argc != 6) {
        cerr << "USAGE: ./sendfile <fileName> <windowSize> <bufferSize> <destinationIP> <destinationPort>" << endl;
        exit(1);
    }

    fileName = argv[1];
    windowSize = atoi(argv[2]);
    maxBufferSize = atoi(argv[3]) * (unsigned int) MAX_DATA_LENGTH;
    ip = argv[4];
    port = atoi(argv[5]);
}

void prepareConnection() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
    	cerr << "Failed to create socket. Terminating..." << endl;
    	exit(1);
    }
    
    server.sin_family = AF_INET;
    hostName = gethostbyname(ip);
    if (hostName == 0) {
    	cerr << "Failed to get hostname. Terminating..." << endl;
    	exit(1);
    }
    
    bcopy((char *)hostName->h_addr, (char *)&server.sin_addr, hostName->h_length);
    server.sin_port = htons(port);
    sockLength = sizeof(struct sockaddr);
    
    if (access(fileName, F_OK) < 0) {
        cerr << "File doesn't exist. Terminating..." << endl;
        exit(-1);
    }

    file = fopen(fileName, "rb");
    buffer = new char[maxBufferSize];
}

void sendFile() {
    thread listenACKThread(listenACK);

    bool isReadDone = false;
    while (!isReadDone) {
        
        bufferSize = fread(buffer, 1, maxBufferSize, file);
        isReadDone = maxBufferSize > bufferSize;
        
        mutexLock.lock();
        hasACKReceived = new bool[windowSize];
        packetSendTime = new timeStamp[windowSize];
        bool hasPacketSend[windowSize];
        int sequenceCount = bufferSize / MAX_DATA_LENGTH;
        if (bufferSize % MAX_DATA_LENGTH) {
            sequenceCount++;
        }

        for (int i = 0; i < windowSize; i++) {
            hasPacketSend[i] = false;
            hasACKReceived[i] = false;
        }

        lastACKReceived = -1;
        lastFrameSent = lastACKReceived + windowSize;
        mutexLock.unlock();

        bool isSendDone = false;
        while (!isSendDone) {
            mutexLock.lock();
            if (hasACKReceived[0]) {
                unsigned int shift = 1;

                for (unsigned int i = 1; i < windowSize; i++) {
                    if (!hasACKReceived[i]) {
                        break;
                    }
                    shift++;
                }

                for (unsigned int i = 0; i < windowSize - shift; i++) {
                    hasPacketSend[i] = hasPacketSend[i + shift];
                    hasACKReceived[i] = hasACKReceived[i + shift];
                    packetSendTime[i] = packetSendTime[i + shift];
                }

                for (unsigned int i = windowSize - shift; i < windowSize; i++) {
                    hasPacketSend[i] = false;
                    hasACKReceived[i] = false;
                }

                lastACKReceived += shift;
                lastFrameSent = lastACKReceived + windowSize;
            }
            mutexLock.unlock();

            for (unsigned int i = 0; i < windowSize; i ++) {
                sequenceNumber = lastACKReceived + i + 1;

                if (sequenceNumber < sequenceCount) {
                    if (!hasPacketSend[i] || ((!hasACKReceived[i]) && (elapsedTime(currentTime(), packetSendTime[i]) > TIMEOUT))) {
                        unsigned int bufferShift = sequenceNumber * MAX_DATA_LENGTH;
                        dataSize = (bufferSize - bufferShift < MAX_DATA_LENGTH) ? (bufferSize - bufferShift) : MAX_DATA_LENGTH;
                        memcpy(data, buffer + bufferShift, dataSize);

                        bool isEOT = (sequenceNumber == sequenceCount - 1) && (isReadDone);
                        packetSize = getPacketSize(packet, sequenceNumber, dataSize, data, isEOT);

                        int n = sendto(sock, packet, packetSize, MSG_WAITALL, (const struct sockaddr *) &server, sockLength);
                        if (n < 0) {
                            cerr << "Failed to send packet. Terminating...\n";
                            exit(1);
                        }

                        hasPacketSend[i] = true;
                        packetSendTime[i] = currentTime();

                        if (!isEOT) {
                            cout << "Packet " << sequenceNumber << " sent." << endl;
                        } else {
                            cout << "EOT Packet " << sequenceNumber << " sent." << endl;
                        }
                    }
                }
            }

            if (lastACKReceived >= sequenceCount - 1) {
                isSendDone = true;
            }
        }
        if (isReadDone) {
            break;
        }
        
    }

    fclose(file);
    delete[] packetSendTime;
    delete[] hasACKReceived;
    listenACKThread.detach();
}

int main(int argc, char *argv[]) {
    readArgument(argc, argv);
    prepareConnection();
    sendFile();
	return 0;
}