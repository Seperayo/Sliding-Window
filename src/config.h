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

// Time
#define TIMEOUT 10
#define current_time chrono::high_resolution_clock::now
#define time_stamp chrono::high_resolution_clock::time_point
#define elapsed_time(end, start) chrono::duration_cast<chrono::milliseconds>(end - start).count()

//Packet
#define MAX_DATA_LENGTH 1024
#define MAX_PACKET_LENGTH 1034
size_t create_packet(char* packet, unsigned int seq_num, size_t data_length, char* data, bool eot);
void read_packet(char* packet, unsigned int* seq_num, size_t* data_length, char* data, bool* is_check_sum_valid, bool* eot);
char count_checksum(size_t data_length, char* data);

//Ack
#define ACK_LENGTH 6
void create_ack(char *ack, unsigned int seq_num, bool is_check_sum_valid);
void read_ack(char *ack, bool* is_nak, unsigned int *seq_num, bool *is_check_sum_valid);

#endif




