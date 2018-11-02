//recvfile.cpp
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <cstdlib>

#include "config.h"

using namespace std;

// Initialize socket variables
int sock, socket_length, window_size;
unsigned int port;
socklen_t fromlen;
struct sockaddr_in server;
struct sockaddr_in from;

// Initialize sliding window variables
int laf, lfr;
int buffer_size, max_buffer_size;
char *buffer;
char *file_name;
FILE *file;
bool eot;

// Initialize packet variables
unsigned int seq_num;
bool is_check_sum_valid;
size_t data_length;
char ack[ACK_LENGTH];
char packet[MAX_PACKET_LENGTH];
char data[MAX_DATA_LENGTH];

void read_packet(char* packet, unsigned int* seq_num, size_t* data_length, char* data, bool* is_check_sum_valid, bool* eot) {
	// convert data
	unsigned int network_seq_num;
	unsigned int network_data_length;

	// byte copy
	memcpy(&network_seq_num, packet+1, 4);
	memcpy(&network_data_length, packet+5, 4);

	*seq_num = ntohl(network_seq_num);
	*data_length = ntohl(network_data_length);

	memcpy(data, packet+9, *data_length);

	char sender_check_sum = packet[9 + *data_length];
	char checksum = count_checksum(*data_length, data);

	*is_check_sum_valid = sender_check_sum == checksum;

	*eot = packet[0] == 0x0;
}

void create_ack(char *ack, unsigned int seq_num, bool is_check_sum_valid){
	
	if(is_check_sum_valid)
		ack[0] = 0x0;
	else
		ack[0] = 0x1;

	uint32_t net_seq_num = htonl(seq_num);
	memcpy(ack + 1, &net_seq_num, 4);
	ack[5] = count_checksum((size_t)4, (char *) &seq_num);
}

void read_argument(int argc, char *argv[]){
	if (argc != 5) {
		cerr << "usage: ./recvfile <file_name> <window_size> <buffer_size> <port>" << endl;
		exit(-1);
	}
	file_name = argv[1];
	window_size = atoi(argv[2]);
	max_buffer_size = (unsigned int)1024 * atoi(argv[3]);
	port = atoi(argv[4]);
}

void prepare_connection(){
	// Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
    	cerr << "Error creating socket" << endl;
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
        cerr << "Can't bind to address" << endl;
        exit(-1);
    }
}

void receive_file(){
	// Open file
	file = fopen(file_name, "wb");

	bool recv_done = false;
	while (!recv_done) {
	    buffer = new char[max_buffer_size];
	    buffer_size = max_buffer_size;

	    int seq_count = max_buffer_size / MAX_DATA_LENGTH;
	    bool packet_received[window_size];
	    for (int i = 0; i < window_size; i++) {
	        packet_received[i] = false;
	    }

	    lfr = -1;
	    laf = lfr + window_size;

	    fromlen = sizeof(struct sockaddr_in);
	    while (true) {
	    	// Block receiving message
	        int packet_size = recvfrom(sock, packet, MAX_PACKET_LENGTH, MSG_WAITALL, (struct sockaddr *)&from, &fromlen);
	        if (packet_size < 0) {
	            cout << "Error on receiving message\n";
	            exit(-1);
	        }

	        // Get packet
	        read_packet(packet, &seq_num, &data_length, data, &is_check_sum_valid, &eot);


	        // Create ack
	        create_ack(ack, seq_num, is_check_sum_valid);
	        // Send ack
	        int ack_size = sendto(sock, ack, ACK_LENGTH, MSG_WAITALL, (struct sockaddr *)&from, fromlen);
	        // if (rand()%10 == 1)
	        // {
	        //     cout << "\n\nLOSS\n\nLOSS\n\n";
	        //     ack_size = -1;
	        // } else {
	        //     ack_size = sendto(sock, ack, ACK_LENGTH, MSG_WAITALL, (struct sockaddr *)&from, fromlen);
	        // }
	        if (ack_size < 0) {
	            cout << "Fail sending ack\n";
	        }
	        if (seq_num <= laf) {
	            if (is_check_sum_valid) {
	                int buffer_shift = seq_num * MAX_DATA_LENGTH;

	                if (seq_num == lfr + 1) {
	                    memcpy(buffer + buffer_shift, data, data_length);
	                    unsigned int shift = 1;
	                    for (unsigned int i = 1; i < window_size; i++) {
	                        if (!packet_received[i]) {
	                            break;
	                        }
	                        shift++;
	                    }
	                    cout << "shift packet_received\n";
	                    for (unsigned int i = 0; i < window_size - shift; i++) {
	                        packet_received[i] = packet_received[i + shift];
	                    }
	                    cout << "false-in packet_received\n";
	                    for (unsigned int i = window_size - shift; i < window_size; i++) {
	                        packet_received[i] = false;
	                    }
	                    lfr += shift;
	                    laf = lfr + window_size;
	                } else if (seq_num > lfr + 1) {
	                    if (!packet_received[seq_num - (lfr + 1)]) {
	                        memcpy(buffer + buffer_shift, data, data_length);
	                        packet_received[seq_num - (lfr + 1)] = true;
	                    }
	                }

	                if (eot) {
	                    buffer_size = buffer_shift + data_length;
	                    seq_count = seq_num + 1;
	                    recv_done = true;
	                    cout << lfr << endl;
	                    cout << "Receive packet eot " << seq_num << endl;
	                    cout << "Sending ack eot " << seq_num << endl;
	                } else {
	                    cout << "Receive packet  " << seq_num << endl;
	                    cout << "Sending ack  " << seq_num << endl;
	                }
	            } else {
	                cout << "ERROR packet " << seq_num << endl;
	                cout << "Sending NAK : " << seq_num << endl;
	            }
	        } else {
	            // Send negative ack
	            cout << "lfr : " << lfr << " laf : " << laf << "\n";
	            cout << "SeqNum out of range : " << seq_num << endl;
	        }

	        if (lfr >= seq_count - 1) {
	            break;
	        }
	    }
	    fwrite(buffer, 1, buffer_size, file);
	}
	fclose(file);
}

int main(int argc, char *argv[]) {
    read_argument(argc, argv);
    prepare_connection();
    receive_file();
    return 0;
}