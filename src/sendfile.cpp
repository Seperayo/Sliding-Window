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
int sock, window_size;
unsigned int sock_length, port;
struct sockaddr_in server, from;
struct hostent *hp;
char *ip;

int lar, lfs;
bool *has_ack_received;
bool *has_packet_send;
time_stamp *packet_send_time;

time_stamp TMIN = current_time();
mutex mt;

// Packet variables
unsigned int seq_num;
unsigned int packet_size, data_size;
char data[MAX_DATA_LENGTH];
char packet[MAX_PACKET_LENGTH];

// File variables
FILE *file;
char *buffer;
char *file_name;
unsigned int max_buffer_size, buffer_size;

size_t create_packet(char* packet, unsigned int seq_num, size_t data_length, char* data, bool eot) {
    // convert data into network type (big endian/little endian)
    unsigned int network_seq_num = htonl(seq_num);
    unsigned int network_data_length = htonl(data_length);

    // copy data into frame
    if (eot) {
        packet[0] = 0x0;
    } else {
        packet[0] = 0x1;
    }

    memcpy(packet+1, &network_seq_num, 4);
    memcpy(packet+5, &network_data_length, 4);
    memcpy(packet+9, data, data_length);

    packet[9 + data_length] = count_checksum(data_length, data);

    return data_length + (size_t)10;
}

void read_ack(char *ack, bool* is_nak, unsigned int *seq_num, bool *is_check_sum_valid){
    
    if(ack[0] == 0x0)
        *is_nak = false;
    else
        *is_nak = true;

    uint32_t net_seq_num;
    memcpy(&net_seq_num, ack + 1, 4);
    *seq_num = ntohl(net_seq_num);

    *is_check_sum_valid = ack[5] == count_checksum((size_t)4, (char *) seq_num);
}

void get_ack() {
    char ack[ACK_LENGTH];
    unsigned int seq_num;
    bool is_check_sum_valid, is_nak;

    while(true) {
        int ack_size = recvfrom(sock, ack, ACK_LENGTH, MSG_WAITALL,(struct sockaddr *)&from, &sock_length);
        if (ack_size < 0) {
            cout << "Packet loss on receiving message" << endl;
            exit(-1);
        }

        // Create ack from buffer
        read_ack(ack, &is_nak, &seq_num, &is_check_sum_valid);

        if (is_check_sum_valid) {
            mt.lock();
            if (seq_num >= lar + 1 && seq_num <= lfs) {
                if (!is_nak) {
                    has_ack_received[seq_num - (lar + 1)] = true;
                    cout << "Receiving ACK : " << seq_num << endl;
                } else {
                    packet_send_time[seq_num - (lar + 1)] = TMIN;
                    cout << "Receiving NAK : " << seq_num << endl;
                }
            } else {
                cout << "ACK out of bound " << seq_num << endl;
            }
            mt.unlock();
        } else {
            cout << "ERROR ACK : " << seq_num << endl;
        }
    }
}

void read_argument(int argc, char *argv[]) {
    if (argc != 6) {
        cerr << "usage: ./sendfile <file_name> <window_size> <buffer_size> <destination_ip> <destination_port>" << endl;
        exit(-1);
    }

    // Get data from argument
    file_name = argv[1];
    window_size = atoi(argv[2]);
    max_buffer_size = atoi(argv[3]) * (unsigned int) MAX_DATA_LENGTH;
    ip = argv[4];
    port = atoi(argv[5]);
}

void prepare_connection() {
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
    	cerr << "ERROR on creating socket" << endl;
    	exit(-1);
    }

    // Get host name
    server.sin_family = AF_INET;
    hp = gethostbyname(ip);
    if (hp == 0) {
    	cerr << "ERROR on getting host name" << endl;
    	exit(-1);
    }

    // Fill server data struct
    bcopy((char *)hp->h_addr, (char *)&server.sin_addr, hp->h_length);
    server.sin_port = htons(port);
    sock_length = sizeof(struct sockaddr_in);

    // Open file
    if (access(file_name, F_OK) < 0) {
        cerr << "FILE not exist\n";
        exit(-1);
    }
    file = fopen(file_name, "rb");
    buffer = new char[max_buffer_size];
}

void send_file() {
    // Create a receiver thread
    thread receiver_thread(get_ack);

    bool read_done = false;
    while (!read_done) {

        // Read file from buffer
        buffer_size = fread(buffer, 1, max_buffer_size, file);
        read_done = max_buffer_size > buffer_size;

        // Initialized sliding window variable
        mt.lock();
        has_ack_received = new bool[window_size];
        packet_send_time = new time_stamp[window_size];
        bool has_packet_send[window_size];
        int seq_count = buffer_size / MAX_DATA_LENGTH;
        if (buffer_size % MAX_DATA_LENGTH) {
            seq_count++;
        }

        for (int i = 0; i < window_size; i++) {
            has_packet_send[i] = false;
            has_ack_received[i] = false;
        }

        lar = -1;
        lfs = lar + window_size;
        mt.unlock();

        bool send_done = false;
        while (!send_done) {
            mt.lock();
            if (has_ack_received[0]) {
                unsigned int shift = 1;

                for (unsigned int i = 1; i < window_size; i++) {
                    if (!has_ack_received[i]) {
                        break;
                    }
                    shift++;
                }

                for (unsigned int i = 0; i < window_size - shift; i++) {
                    has_packet_send[i] = has_packet_send[i + shift];
                    has_ack_received[i] = has_ack_received[i + shift];
                    packet_send_time[i] = packet_send_time[i + shift];
                }

                for (unsigned int i = window_size - shift; i < window_size; i++) {
                    has_packet_send[i] = false;
                    has_ack_received[i] = false;
                }

                lar += shift;
                lfs = lar + window_size;
            }
            mt.unlock();

            for (unsigned int i = 0; i < window_size; i ++) {
                seq_num = lar + i + 1;

                if (seq_num < seq_count) {
                    if (!has_packet_send[i] || ((!has_ack_received[i]) && (elapsed_time(current_time(), packet_send_time[i]) > TIMEOUT))) {
                        unsigned int buffer_shift = seq_num * MAX_DATA_LENGTH;
                        data_size = (buffer_size - buffer_shift < MAX_DATA_LENGTH) ? (buffer_size - buffer_shift) : MAX_DATA_LENGTH;
                        memcpy(data, buffer + buffer_shift, data_size);

                        bool eot = (seq_num == seq_count - 1) && (read_done);
                        packet_size = create_packet(packet, seq_num, data_size, data, eot);

                        int n = sendto(sock, packet, packet_size, MSG_WAITALL, (const struct sockaddr *) &server, sock_length);
                        if (n < 0) {
                            cerr << "ERROR sending packet\n";
                            exit(-1);
                        }

                        has_packet_send[i] = true;
                        packet_send_time[i] = current_time();

                        if (!eot) {
                            cout << "Sending package " << seq_num << endl;
                        } else {
                            cout << "Sending eot package " << seq_num << endl;
                        }
                    }
                }
            }

            if (lar >= seq_count - 1) {
                send_done = true;
            }
        }
        cout << "is read done ? " << read_done << "\n";
        if (read_done) {
            break;
        }
    }

    fclose(file);
    delete[] packet_send_time;
    delete[] has_ack_received;
    receiver_thread.detach();
}

int main(int argc, char *argv[]) {
    read_argument(argc, argv);
    prepare_connection();
    send_file();
	return 0;
}