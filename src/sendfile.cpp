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

int lar, lfs;
bool *hasACKReceived;
bool *hasPacketSend;
time_stamp *packetSendTime;

time_stamp TMIN = current_time();
mutex mt;

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

size_t getPacketSize(char* packet, unsigned int sequenceNumber, size_t dataLength, char* data, bool eot) {
    // convert data into network type (big endian/little endian)
    unsigned int networkSequenceNumber = htonl(sequenceNumber);
    unsigned int networkDataLength = htonl(dataLength);

    // copy data into frame
    if (eot) {
        packet[0] = 0x0;
    } else {
        packet[0] = 0x1;
    }

    memcpy(packet+1, &networkSequenceNumber, 4);
    memcpy(packet+5, &networkDataLength, 4);
    memcpy(packet+9, data, dataLength);

    packet[9 + dataLength] = count_checksum(dataLength, data);

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

    *isChecksumValid = ACK[5] == count_checksum((size_t)4, (char *) sequenceNumber);
}

void listenACK() {
    char ACK[ACK_LENGTH];
    unsigned int sequenceNumber;
    bool isChecksumValid, isNAK;

    while(true) {
        int ACKSize = recvfrom(sock, ACK, ACK_LENGTH, MSG_WAITALL,(struct sockaddr *)&from, &sockLength);
        if (ACKSize < 0) {
            cout << "Packet loss on receiving message" << endl;
            exit(1);
        }

        // Create ACK from buffer
        readACK(ACK, &isNAK, &sequenceNumber, &isChecksumValid);

        if (isChecksumValid) {
            mt.lock();
            if (sequenceNumber >= lar + 1 && sequenceNumber <= lfs) {
                if (!isNAK) {
                    hasACKReceived[sequenceNumber - (lar + 1)] = true;
                    cout << "Receiving ACK: " << sequenceNumber << endl;
                } else {
                    packetSendTime[sequenceNumber - (lar + 1)] = TMIN;
                    cout << "Receiving NAK: " << sequenceNumber << endl;
                }
            } else {
                cout << "ACK out of bound " << sequenceNumber << endl;
            }
            mt.unlock();
        } else {
            cout << "ACK corrupt" << sequenceNumber << endl;
        }
    }
}

void readArgument(int argc, char *argv[]) {
    if (argc != 6) {
        cerr << "USAGE: ./sendfile <fileName> <windowSize> <bufferSize> <destinationIP> <destinationPort>" << endl;
        exit(1);
    }

    // Get data from argument
    fileName = argv[1];
    windowSize = atoi(argv[2]);
    maxBufferSize = atoi(argv[3]) * (unsigned int) MAX_DATA_LENGTH;
    ip = argv[4];
    port = atoi(argv[5]);
}

void prepareConnection() {
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
    	cerr << "ERROR: creating socket" << endl;
    	exit(1);
    }

    // Get host name
    server.sin_family = AF_INET;
    hostName = gethostbyname(ip);
    if (hostName == 0) {
    	cerr << "ERROR: getting host name" << endl;
    	exit(1);
    }

    // Fill server data struct
    bcopy((char *)hostName->h_addr, (char *)&server.sin_addr, hostName->h_length);
    server.sin_port = htons(port);
    sockLength = sizeof(struct sockaddr);

    // Open file
    if (access(fileName, F_OK) < 0) {
        cerr << "ERROR: File did not exist" << endl;
        exit(-1);
    }
    file = fopen(fileName, "rb");
    buffer = new char[maxBufferSize];
}

void sendFile() {
    // Create a receiver thread
    thread receiver_thread(listenACK);

    bool read_done = false;
    while (!read_done) {

        // Read file from buffer
        bufferSize = fread(buffer, 1, maxBufferSize, file);
        read_done = maxBufferSize > bufferSize;

        // Initialized sliding window variable
        mt.lock();
        hasACKReceived = new bool[windowSize];
        packetSendTime = new time_stamp[windowSize];
        bool hasPacketSend[windowSize];
        int sequenceCount = bufferSize / MAX_DATA_LENGTH;
        if (bufferSize % MAX_DATA_LENGTH) {
            sequenceCount++;
        }

        for (int i = 0; i < windowSize; i++) {
            hasPacketSend[i] = false;
            hasACKReceived[i] = false;
        }

        lar = -1;
        lfs = lar + windowSize;
        mt.unlock();

        bool send_done = false;
        while (!send_done) {
            mt.lock();
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

                lar += shift;
                lfs = lar + windowSize;
            }
            mt.unlock();

            for (unsigned int i = 0; i < windowSize; i ++) {
                sequenceNumber = lar + i + 1;

                if (sequenceNumber < sequenceCount) {
                    if (!hasPacketSend[i] || ((!hasACKReceived[i]) && (elapsed_time(current_time(), packetSendTime[i]) > TIMEOUT))) {
                        unsigned int buffer_shift = sequenceNumber * MAX_DATA_LENGTH;
                        dataSize = (bufferSize - buffer_shift < MAX_DATA_LENGTH) ? (bufferSize - buffer_shift) : MAX_DATA_LENGTH;
                        memcpy(data, buffer + buffer_shift, dataSize);

                        bool eot = (sequenceNumber == sequenceCount - 1) && (read_done);
                        packetSize = getPacketSize(packet, sequenceNumber, dataSize, data, eot);

                        int n = sendto(sock, packet, packetSize, MSG_WAITALL, (const struct sockaddr *) &server, sockLength);
                        if (n < 0) {
                            cerr << "ERROR sending packet\n";
                            exit(1);
                        }

                        hasPacketSend[i] = true;
                        packetSendTime[i] = current_time();

                        if (!eot) {
                            cout << "Sending package " << sequenceNumber << endl;
                        } else {
                            cout << "Sending eot package " << sequenceNumber << endl;
                        }
                    }
                }
            }

            if (lar >= sequenceCount - 1) {
                send_done = true;
            }
        }
        if (read_done) {
            break;
        }
        
    }

    fclose(file);
    delete[] packetSendTime;
    delete[] hasACKReceived;
    receiver_thread.detach();
}

int main(int argc, char *argv[]) {
    readArgument(argc, argv);
    prepareConnection();
    sendFile();
	return 0;
}