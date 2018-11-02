//config.cpp
#include "config.h"

char count_checksum(size_t dataLength, char* data) {
	unsigned long sum = 0;
	for (size_t i = 0; i < dataLength; i++) {
		sum += *data++;
		if (sum & 0xFF00) {
			sum &= 0xFF;
			sum++;
		}
	}
	return (char)(~(sum & 0xFF));
}