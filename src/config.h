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
#define ACK_LENGTH 6
#define current_time chrono::high_resolution_clock::now
#define time_stamp chrono::high_resolution_clock::time_point
#define elapsed_time(end, start) chrono::duration_cast<chrono::milliseconds>(end - start).count()
char count_checksum(size_t data_length, char* data);


#endif




