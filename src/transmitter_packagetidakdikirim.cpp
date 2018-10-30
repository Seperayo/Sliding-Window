#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <netdb.h>
#include <thread>
#include <arpa/inet.h>
#include <algorithm>
#include <fstream>
#include "frame.h"

using namespace std;

#define DELAY 1

struct sockaddr_in receiverAddress; // Alamat
socklen_t receiverLength = sizeof(receiverAddress);
int sockfd; // Socket
int windowHead = 0;	// variabel yang menyimpan nomor frame yang terletak di window paling depan
int window[WINDOWSIZE] = {0};	// array of int yang menampung status frame pada window. 0 artinya perlu dikirim, -1 artinya menunggu timeout, 1 artinya sudah ACK
char data[MAXLEN];	// Array of char yang menampung karakter dari file
int nData = 0;	// Jumlah elemen pada array of char data
Frame frames[100];	// array of Frame yang menampung semua frame yang dimiliki transmitter. frames[0] berisi frame dengan nomor 1, dst
int frameNumber = 0;	// Banyaknya frame
bool endFrame = false;	// True jika semua frame yang ada sudah ACK
bool endWindow = false;	// True jika window tidak bisa bergeser lagi
int timeOut[WINDOWSIZE] = {0};	// array of int yang menampung waktu untuk mengatur timeout tiap frame di window

void initiate(char *server, char *servPort);
void initiate2(char *server, char *servPort);
void readFile(char *fileinput);
void DieWithSystemMessage(const char *msg);
void DieWithUserMessage(const char *msg, const char *detail);
void splitDataToFrames();
void sendFrame();
void receiveAckNak();
void waitTimeOut();

int main(int argc, char *argv[]){
	int input = 0;

	initiate2(argv[1],argv[2]);
	while ((input != 1) && (input != 2)) {
		cout << "1. Masukan dari pengguna" << endl;
		cout << "2. Masukan dari file" << endl;
		cout << "Pilihan (1/2): ";
		cin >> input;
		if (input == 1) {
			string userInput;
			cout << "Masukkan data: ";
			cin.ignore();
			getline(cin,userInput);
			int z;
			for (z = 0; z < userInput.length(); z++) {
				data[z] = userInput[z];
			}
			data[z-1] = '\0';
		} else if (input == 2) {
			readFile(argv[3]);
		}
	}

	splitDataToFrames();

	// Membuat dan mensinkronisasikan thread
	thread thread1 (sendFrame);
	thread thread2 (receiveAckNak);
	//thread thread3 (waitTimeOut);
	thread1.join();
	thread2.join();
	//thread3.join();

	return 0;
}

void initiate(char *server, char *servPort){
	int sock;
	// address info
	struct addrinfo addrCriteria;
	memset(&addrCriteria, 0, sizeof(addrCriteria));
	addrCriteria.ai_family = AF_UNSPEC;

	addrCriteria.ai_socktype = SOCK_DGRAM;
	addrCriteria.ai_protocol = IPPROTO_UDP;
	
	struct addrinfo *servAddr; // List of server addresses
	int rtnVal = getaddrinfo(server, servPort, &addrCriteria, &servAddr);
	if (rtnVal != 0)
	DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
	// Create a datagram/UDP socket
	sock = socket(servAddr->ai_family, servAddr->ai_socktype,
	servAddr->ai_protocol); // Socket descriptor for client
	if (sock < 0)
	//printf("socket() failed");
	DieWithSystemMessage("socket() failed");
}

void initiate2(char *server, char *servPort) {
	// Membuat UDP socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		cout << "Socket tidak dapat dibuat" << endl;
		exit(1);
	} else {
		cout << "Membuat socket untuk koneksi ke " << server << ":" << servPort << " ..." << endl;
	}
	// Alamat socket
	struct sockaddr_in transmitterAddress;
	receiverAddress.sin_family = AF_INET;
	receiverAddress.sin_port = htons(atoi(servPort));
	receiverAddress.sin_addr.s_addr = inet_addr(server);
	memset(receiverAddress.sin_zero,'\0',sizeof receiverAddress.sin_zero);
	if (transmitterAddress.sin_addr.s_addr == INADDR_NONE) {
		cout << "IP Address tidak ada" << endl;
		exit(1);
	}
}

void readFile(char *fileinput){
	ifstream textFile;
	int i = 0;
	char c;
	// Membuka file
	textFile.open(fileinput);
	if (!textFile.is_open()) {
		cout << "File tidak ada" << endl;
		exit(1);
	}
	while (!textFile.eof()) {
		textFile.get(c);
		data[i] = c;
		i++;
	}
	data[i-1] = '\0';
	cout << "Isi pesan: " << data << endl << endl;
}

void DieWithUserMessage(const char *msg, const char *detail) {
	fputs(msg, stderr);
	fputs(": ", stderr);
	fputs(detail, stderr);
	fputc('\n', stderr);
	exit(1);
}

void DieWithSystemMessage(const char *msg) {
	perror(msg);
	exit(1);
}

void splitDataToFrames() {
	int i = 0;
	nData = strlen(data);
	cout << strlen(data) << endl;
	while (i < nData) {
		if ((nData - i) >= DATALENGTH) {
			char tempData[DATALENGTH];
			for (int j = 0; j < DATALENGTH; j++) {
				tempData[j] = data[i];
				i++;
			}
			setDataToFrame(tempData, frameNumber, frames[frameNumber]);
			frameNumber++;
		} else {
			int n;

			n = nData - i;
			char tempData[n];
			for (int j = 0; j < n; j++) {
				tempData[j] = data[i];
				i++;
			}
			setDataToFrame(tempData, frameNumber, frames[frameNumber]);
			//cout << "CS: " << frames[frameNumber].checkSum << endl;
			frameNumber++;
			//delete [] tempData;
		}
	}
	if (frameNumber < WINDOWSIZE) {
		endWindow = true;
		for (int j = frameNumber; j < WINDOWSIZE; j++) {
			window[j] = 1;
		}
	}
}

void sendFrame() {
	cout << "Window berada di Frame " << windowHead+1 << " - " << windowHead+WINDOWSIZE << endl;
	while (!endFrame) {
		for (int i = 0; i < WINDOWSIZE; i++) {
			// Jika frame belum dikirim
			if (window[i] == 0) {
				char message[100];
				//cout << "CS frame: " << frames[windowHead+i].checkSum << endl;
				setFrameToPointer(frames[windowHead+i], message);
				if (i != 3) {
					sendto(sockfd, message, MAXLEN, 0, (struct sockaddr *) &receiverAddress, receiverLength);
				}
				window[i] = -1;
				cout << "Mengirim Frame ke " << windowHead+i+1 << ": " << frames[windowHead+i].data << endl;
			}
		}
	}
}

void receiveAckNak() {
	char ackChar[100];
	int rec;
	Ack receiverAck;

	while (!endFrame) {
		sleep(DELAY);
		rec = recvfrom(sockfd, ackChar, MAXLEN, 0, (struct sockaddr *) &receiverAddress, (socklen_t *) &receiverLength);
		if (rec < 0) {
			perror("Pembacaan data dari receiver tidak berhasil");
		}
		setPointerToAck(ackChar, receiverAck);
		if (isAckNak(getAck(receiverAck))) {
			window[getFrameNumber(receiverAck) - windowHead] = 1;
			cout << "Frame " << getFrameNumber(receiverAck)+1 << " ACK" << endl;

			// Jika window sudah tidak bisa di-slide
			if (endWindow) {
				// Jika semua frame di window sudah ACK, maka selesai
				endFrame = all_of(begin(window), end(window), [](int w){return w==1;});
			} else {
				// Jika frame paling awal di window sudah ACK, maka slide
				if (window[0] == 1) {
					if ((windowHead+WINDOWSIZE) > (frameNumber-1)) {
						endWindow = true;
					} else {
						// Window geser
						for (int i = 1; i < WINDOWSIZE; i++) {
							window[i-1] = window[i];
							timeOut[i-1] = timeOut[i];
						}
						window[WINDOWSIZE-1] = 0;
						timeOut[WINDOWSIZE-1] = 0;
						windowHead++;
						cout << "Window berada di Frame " << windowHead+1 << " - " << windowHead+WINDOWSIZE << endl;
					}
				}
			}
		} else { // NAK
			window[getAck(receiverAck) - windowHead] = 0;
			cout << "Frame " << getFrameNumber(receiverAck)+1 << " NAK" << endl;
		}
	}
}

void waitTimeOut() {
	while (!endFrame) {
		sleep(DELAY);
		for (int i = 0; i < WINDOWSIZE; i++) {
			if (timeOut[i] >= 1) {
				timeOut[i] = 0;
				window[i] = 0;
				cout << "Frame " << windowHead+i+1 << " timeout" << endl;
			}
			if (window[i] == -1) {
				(timeOut[i])++;
			}
		}
	}
}