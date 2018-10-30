#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "frame.h"

using namespace std;

// Getter
unsigned int getSoh(Frame frame) {
	return frame.soh;
}
unsigned int getFrameNumber(Frame frame) {
	return frame.frameNumber;
}
char getData(Frame frame, int idx) {
	return frame.data[idx];
}
unsigned int getDataSize(Frame frame) {
	char stringFrame[DATALENGTH];
	setFrameToString(frame, stringFrame);
	return strlen(stringFrame);
}
unsigned int getCheckSum(Frame frame) {
	return frame.checkSum;
}
unsigned int getAck(Ack ack) {
	return ack.ack;
}
unsigned int getFrameNumber(Ack ack) {
	return ack.frameNumber;
}
unsigned int getCheckSum(Ack ack) {
	return ack.checkSum;
}

// Fungsi dan prosedur lainnya
bool isAckNak(unsigned int x) {
	return (x == ACK);
}
bool isFrameValid(Frame frame) {
	return (frame.checkSum == countCheckSum(frame));
}
bool isFrameEmpty(Frame frame) {
	return ((frame.soh==EMPTY) && (frame.frameNumber==EMPTY) && (frame.datasize==EMPTY) && (frame.data[0]=='\0') && (frame.checkSum==EMPTY));
}
int countCheckSum(Frame frame) {
	char stringFrame[100];
	setFrameToString(frame, stringFrame);
	stringFrame[strlen(stringFrame)] = ETX;
	unsigned long long checksum = 0;
	unsigned long long CRC= 0x14B;
	int i=0;
	do{
		checksum = checksum << 8;
		checksum +=  (unsigned long long)  stringFrame[i];
		i++;
	}while(stringFrame[i]!= ETX);
	checksum = (checksum <<8);
	for(int i = sizeof(long long)*8; i>8; i--){
		if((checksum>>(i-1))&1){
			checksum = checksum ^ (CRC<<(i-9));
		}
	}
	return (int) checksum;
}
//
int countCheckSum(char *stringFrame) {
	unsigned long long checksum = 0;
	unsigned long long CRC= 0x14B;
	int i=0;
	do{
		checksum = checksum <<8;
		checksum +=  (unsigned long long)  stringFrame[i];
		i++;
	}while(stringFrame[i]!= ETX);
	checksum = (checksum <<8);
	for(int i = sizeof(long long)*8; i>8; i--){
		if((checksum>>(i-1))&1){
			checksum = checksum ^ (CRC<<(i-9));
		}
	}
	return (int) checksum;
}

// Setter
void setEmptyFrame(Frame &frame) {
	frame.soh = EMPTY;
	frame.frameNumber = EMPTY;
	frame.datasize = EMPTY;
	frame.data[0] = '\0';
	frame.checkSum = EMPTY;
}
void setDataToFrame(char *data, unsigned int frameNumber, Frame &frame) {
	frame.soh = SOH;
	frame.frameNumber = frameNumber;
	frame.datasize = strlen(data);
	for (int i = 0; i < strlen(data); i++) {
		frame.data[i] = data[i];
	}
	frame.checkSum = countCheckSum(frame);
}
void setFrameToPointer(Frame frame, char *message) {
	sprintf(message, "%d||%d||%d||%s||%d||", frame.soh, frame.frameNumber, frame.datasize, frame.data, frame.checkSum);
}
void setPointerToFrame(char *message, Frame &frame) {
	int i = 0, iStart = 0;
	char temp[strlen(message)];
	for (int nPartition = 0; nPartition < 6; nPartition++) {
		while (message[i] != '|' || message[i+1] != '|') {
			i++;
		}
		// Copy char to frame
		int k = 0;
		for (int j = iStart; j < i; j++) {
			temp[k] = message[j];
			temp[k+1] = '\0';
			k++;
		}
		if (nPartition == 0) {
			// SOH
			frame.soh = atoi(temp);
		} else if (nPartition == 1) {
			// frameNumber
			frame.frameNumber = strlen(temp);
		} else if (nPartition == 2) {
			frame.datasize
		} else if (nPartition == 3) {
			// Data
			//frame.data = new char[strlen(temp)];
			strcpy(frame.data, temp);
		} else {
			// checkSum
			frame.checkSum = atoi(temp);
		}
		temp[0] = '\0';
		iStart = i+2;
		i = iStart;
	}
}
void setFrameToString(Frame frame, char *word) {
	sprintf(word, "%d%d%d%s", frame.soh, frame.frameNumber, frame.datasize, frame.data);
}
void setAck(unsigned int ackValue, unsigned int frameNumber, int checkSum, Ack &ack) {
	ack.ack = ackValue;
	ack.frameNumber = frameNumber;
	ack.checkSum = checkSum;
}
void setAckToPointer(Ack ack, char *message) {
	sprintf(message, "%d||%d||%d||", ack.ack, ack.frameNumber, ack.checkSum);
}
void setPointerToAck(char *message, Ack &ack) {
	int i = 0, iStart = 0;
	char temp[strlen(message)];
	for (int nPartition = 0; nPartition < 3; nPartition++) {
		while (message[i] != '|' || message[i+1] != '|') {
			i++;
		}
		// Copy char to ack
		int k = 0;
		for (int j = iStart; j < i; j++) {
			temp[k] = message[j];
			temp[k+1] = '\0';
			k++;
		}
		if (nPartition == 0) {
			ack.ack = atoi(temp);
		} else if (nPartition == 1) {
			ack.frameNumber = atoi(temp);
		} else {
			ack.checkSum = atoi(temp);
		}
		temp[0] = '\0';
		iStart = i+2;
		i = iStart;
	}
}
