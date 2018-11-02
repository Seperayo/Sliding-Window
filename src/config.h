//config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <chrono>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <iostream>
using namespace std;

#define TIMEOUT 10
#define MAX_DATA_LENGTH 1024
#define MAX_PACKET_LENGTH 1034
#define ACK_TIME 5000
#define ACK_LENGTH 6
#define currentTime chrono::high_resolution_clock::now
#define timeStamp chrono::high_resolution_clock::time_point
#define elapsedTime(end, start) chrono::duration_cast<chrono::milliseconds>(end - start).count()
char countChecksum(size_t dataLength, char* data);


#endif




