all:
	g++ src/receiver.cpp -o receiver -std=c++0x -pthread
	g++ src/transmitter.cpp -o transmitter -std=c++0x -pthread
	
receiver:
	g++ src/receiver.cpp -o receiver -std=c++0x -pthread

transmitter:
	g++ src/transmitter.cpp -o transmitter -std=c++0x -pthread

clean:
	rm -f receiver transmitter

cleanreceiver:
	rm -f receiver

cleantransmitter:
	rm -f transmitter