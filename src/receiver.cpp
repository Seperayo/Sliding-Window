#include <iostream>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include "frame.cpp"
#include "stdbool.h"

using namespace std;

#define CONSUME_DELAY 0
#define RECEIVE_DELAY 2

/* Socket */
int sockfd; // listen on sock_fd
struct sockaddr_in receiverAddress, remoteAddress; // Alamat 
socklen_t addrlen = sizeof(remoteAddress);
Frame buffer[BUFFERSIZE];
Frame queue[BUFFERSIZE];
int iBuffer = 0, iConsumed = 0, nSlide = 0;

/* Functions declaration */
void initiate(char *servPort);
void initiate2(char *portNumber);
void receiveFrame();
void consumeFrame();
void sendAckNak(unsigned int ackValue, Frame frame);
void moveQueueToBuffer();

int main(int argc, char *argv[])
{
	// Memeriksa parameter
	if (argc < 2) {
		cout << "Error, tidak ada port yang disediakan" << endl;
		exit(1);
	}
	initiate(argv[1]);

	// Inisiasi isi queue
	for (int i = 0; i < BUFFERSIZE; i++) {
		setEmptyFrame(queue[i]);
		setEmptyFrame(buffer[i]);
	}

	thread parentThread(receiveFrame);
	thread childThread(consumeFrame);
	parentThread.join();
	childThread.join();

	close(sockfd);
	return 0;
}

void initiate(char *portNumber) {
	// Membuat UDP socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		// Socket bernilai -1 jika gagal dibuat
		cout << "Socket tidak dapat dibuat" << endl;
		exit(1);
	}

	// Bind socket ke alamat IP dan port
	int portNo = atoi(portNumber);
	receiverAddress.sin_family = AF_INET;
	receiverAddress.sin_port = htons(portNo);
	receiverAddress.sin_addr.s_addr = INADDR_ANY;
	if (bind(sockfd, (struct sockaddr *)&receiverAddress, sizeof(receiverAddress)) < 0) {
		cout << "Binding gagal" << endl;
		close(sockfd);
		exit(1);
	} else {
		cout << "Binding pada " << INADDR_ANY << ":" << portNo << endl;
	}
}

Frame frameTemp;
void receiveFrame() {
	// Kamus Lokal
	int recvlen; // Jumlah bytes yang diterima
	char data[MAXLEN]; // Data dari transmitter
	int nReceived = 0; // Jumlah byte yang diterima
	int nFrame = 0;
	Frame frame;

	// Algoritma
	recvlen = recvfrom(sockfd, data, MAXLEN, 0, (struct sockaddr *) &remoteAddress, (socklen_t *) &addrlen);
	while (true) {
		// Ubah pesan menjadi frame
		setPointerToFrame(data, frame);
		nFrame = nSlide*WINDOWSIZE+getFrameNumber(frame)+1;
		cout << "Menerima frame ke-" << nFrame << ": " << frame.data << endl;
		cout << "Checksum receiver " << frame.checkSum << endl;
		// Periksa apakah data valid
		if (isFrameValid(frame)) {
			// Kirim ACK
			sendAckNak(ACK, frame);
			sendAckNak(ACK, frame);
			cout << "Mengirim ACK frame ke-" << nFrame << endl;
			cout << "Window berada di frame " << nFrame << " - " << nFrame+(WINDOWSIZE-1) << endl << endl;
			if (iBuffer == getFrameNumber(frame)-WINDOWSIZE*nSlide) {
				setDataToFrame(frame.data, getFrameNumber(frame), buffer[iBuffer]);
				// Increment indeks buffer
				if (iBuffer == WINDOWSIZE-1) {
					iBuffer = 0;
					nSlide++;
				} else {
					iBuffer++;
				}
			} else {
				setDataToFrame(frame.data, getFrameNumber(frame), queue[getFrameNumber(frame)]);
			}
			moveQueueToBuffer();
			sleep(RECEIVE_DELAY);
		} else {
			sendAckNak(NAK, frame);
			cout << "Mengirim NAK frame ke-" << nFrame << endl << endl;
		}
		nReceived++;
		recvlen = recvfrom(sockfd, data, MAXLEN, 0, (struct sockaddr *) &remoteAddress, (socklen_t *) &addrlen);
	}
	// Mengirim sinyal bahwa program telah berakhir
}

void consumeFrame() {
	Frame f;
	int nConsumed = 0;
	int nFrame = 0;

	while (true) {
		if (!isFrameEmpty(buffer[iConsumed])) {
			nFrame = nSlide*WINDOWSIZE+getFrameNumber(buffer[iConsumed])+1;
			cout << "Mengkonsumsi frame ke-" << nFrame << ": " << buffer[iConsumed].data << endl << endl;
			setEmptyFrame(buffer[iConsumed]);
			// Increment indeks buffer
			if (iConsumed == WINDOWSIZE-1) {
				iConsumed = 0;
			} else {
				iConsumed++;
			}
			nConsumed++;
		}
		sleep(CONSUME_DELAY);
	}
}

void sendAckNak(unsigned int ackValue, Frame frame) {
	Ack ackMessage;
	char message[MAXLEN];
	setAck(ackValue, getFrameNumber(frame), getCheckSum(frame), ackMessage);
	setAckToPointer(ackMessage, message);
	sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &remoteAddress, addrlen);
}

void moveQueueToBuffer() {
	// Cari indeks di queue yang terisi
	int i = 0;
	bool isFound = false;
	while (!isFound && (i < BUFFERSIZE)) {
		if (isFrameEmpty(queue[i])) {
			isFound = true;
		} else {
			i++;
		}
	}
	if (iBuffer == i) {
		// Pindahkan isi queue ke buffer
		while (isFrameEmpty(queue[i]) && (i < BUFFERSIZE)) {
			setDataToFrame(queue[i].data, getFrameNumber(queue[i]), buffer[i]);
			setEmptyFrame(queue[i]);
			i++;
		}
	}
}
