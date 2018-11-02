//recvfile.cpp
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <cstdlib>

#include "config.h"

using namespace std;

// Initialize socket variables
int sock, socket_length, windowSize;
unsigned int port;
socklen_t fromlen;
struct sockaddr_in server, from;

// Initialize sliding window variables
int largestAcceptableFrame, lastFrameReceived;
int bufferSize, maxBufferSize;
char *buffer;
char *fileName;
FILE *file;
bool eot;

// Initialize packet variables
unsigned int sequenceNumber;
bool isChecksumValid;
size_t dataLength;
char ACK[ACK_LENGTH];
char packet[MAX_PACKET_LENGTH];
char data[MAX_DATA_LENGTH];

void readPacket(char* packet, unsigned int* sequenceNumber, size_t* dataLength, char* data, bool* isChecksumValid, bool* eot) {
	// convert data
	unsigned int networkSequenceNumber;
	unsigned int networkDataLength;

	// byte copy
	memcpy(&networkSequenceNumber, packet+1, 4);
	memcpy(&networkDataLength, packet+5, 4);

	*sequenceNumber = ntohl(networkSequenceNumber);
	*dataLength = ntohl(networkDataLength);

	memcpy(data, packet+9, *dataLength);

	char sender_check_sum = packet[9 + *dataLength];
	char checksum = count_checksum(*dataLength, data);

	*isChecksumValid = sender_check_sum == checksum;

	*eot = packet[0] == 0x0;
}

void createACK(char *ACK, unsigned int sequenceNumber, bool isChecksumValid){
	
	if(isChecksumValid)
		ACK[0] = 0x0;
	else
		ACK[0] = 0x1;

	uint32_t networkSequenceNumber = htonl(sequenceNumber);
	memcpy(ACK + 1, &networkSequenceNumber, 4);
	ACK[5] = count_checksum((size_t)4, (char *) &sequenceNumber);
}

void readArgument(int argc, char *argv[]){
	if (argc != 5) {
		cerr << "USAGE: ./recvfile <fileName> <windowSize> <bufferSize> <port>" << endl;
		exit(-1);
	}
	fileName = argv[1];
	windowSize = atoi(argv[2]);
	maxBufferSize = (unsigned int)1024 * atoi(argv[3]);
	port = atoi(argv[4]);
}

void prepareConnection(){
	// Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
    	cerr << "ERROR: Failed creating socket" << endl;
    	exit(-1);
    }

    // Assign server port and address
    socket_length = sizeof(server);
    bzero(&server, socket_length);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    // Bind socket
    if (bind(sock, (struct sockaddr *) &server, socket_length) < 0) {
        cerr << "ERROR: Can't bind to address" << endl;
        exit(-1);
    }
}

void receiveFile(){
	// Open file
	file = fopen(fileName, "wb");

	bool isReceiveDone = false;
	while (!isReceiveDone) {
	    buffer = new char[maxBufferSize];
	    bufferSize = maxBufferSize;

	    int sequenceCount = maxBufferSize / MAX_DATA_LENGTH;
	    bool isPacketReceived[windowSize];
	    for (int i = 0; i < windowSize; i++) {
	        isPacketReceived[i] = false;
	    }

	    lastFrameReceived = -1;
	    largestAcceptableFrame = lastFrameReceived + windowSize;

	    fromlen = sizeof(struct sockaddr_in);
	    while (true) {
	    	// Block receiving message
	        int packetSize = recvfrom(sock, packet, MAX_PACKET_LENGTH, MSG_WAITALL, (struct sockaddr *)&from, &fromlen);
	        if (packetSize < 0) {
	            cerr << "ERROR: Packet size is less than 0" << endl;
	            exit(-1);
	        }

	        // Get packet
	        readPacket(packet, &sequenceNumber, &dataLength, data, &isChecksumValid, &eot);

	        // Create ACK
	        createACK(ACK, sequenceNumber, isChecksumValid);
	        // Send ACK
	        int ACKSize = sendto(sock, ACK, ACK_LENGTH, MSG_WAITALL, (struct sockaddr *)&from, fromlen);
	        // if (rand()%10 == 1)
	        // {
	        //     cout << "\n\nLOSS\n\nLOSS\n\n";
	        //     ACKSize = -1;
	        // } else {
	        //     ACKSize = sendto(sock, ACK, ACK_LENGTH, MSG_WAITALL, (struct sockaddr *)&from, fromlen);
	        // }
	        if (ACKSize < 0) {
	            cout << "Fail sending ACK\n";
	        }
	        if (sequenceNumber <= largestAcceptableFrame) {
	            if (isChecksumValid) {
	                int buffer_shift = sequenceNumber * MAX_DATA_LENGTH;

	                if (sequenceNumber == lastFrameReceived + 1) {
	                    memcpy(buffer + buffer_shift, data, dataLength);
	                    unsigned int shift = 1;
	                    for (unsigned int i = 1; i < windowSize; i++) {
	                        if (!isPacketReceived[i]) {
	                            break;
	                        }
	                        shift++;
	                    }

	                    for (unsigned int i = 0; i < windowSize - shift; i++) {
	                        isPacketReceived[i] = isPacketReceived[i + shift];
	                    }

	                    for (unsigned int i = windowSize - shift; i < windowSize; i++) {
	                        isPacketReceived[i] = false;
	                    }
	                    lastFrameReceived += shift;
	                    largestAcceptableFrame = lastFrameReceived + windowSize;
	                } else if (sequenceNumber > lastFrameReceived + 1) {
	                    if (!isPacketReceived[sequenceNumber - (lastFrameReceived + 1)]) {
	                        memcpy(buffer + buffer_shift, data, dataLength);
	                        isPacketReceived[sequenceNumber - (lastFrameReceived + 1)] = true;
	                    }
	                }

	                if (eot) {
	                    bufferSize = buffer_shift + dataLength;
	                    sequenceCount = sequenceNumber + 1;
	                    isReceiveDone = true;
	                    cout << "Receive packet eot " << sequenceNumber << endl;
	                    cout << "Sending ACK eot " << sequenceNumber << endl;
	                } else {
	                    cout << "Receive packet  " << sequenceNumber << endl;
	                    cout << "Sending ACK  " << sequenceNumber << endl;
	                }
	            } else {
	                cout << "ERROR packet " << sequenceNumber << endl;
	                cout << "Sending NAK : " << sequenceNumber << endl;
	            }
	        } else {
	            cout << "lastFrameReceived : " << lastFrameReceived << " largestAcceptableFrame : " << largestAcceptableFrame << "\n";
	            cout << "SeqNum out of range : " << sequenceNumber << endl;
	        }

	        if (lastFrameReceived >= sequenceCount - 1) {
	            break;
	        }
	    }
	    fwrite(buffer, 1, bufferSize, file);
	}
	fclose(file);
}

int main(int argc, char *argv[]) {
    readArgument(argc, argv);
    prepareConnection();
    receiveFile();
    return 0;
}