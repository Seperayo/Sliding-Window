all: recvfile sendfile

recvfile: src/config.h src/config.cpp src/recvfile.cpp
	g++ -pthread src/config.cpp src/recvfile.cpp -o recvfile -std=c++11 

sendfile: src/config.h src/config.cpp src/sendfile.cpp
	g++ -pthread src/config.cpp src/sendfile.cpp -o sendfile -std=c++11 

clean: recvfile sendfile
	rm -f src/recvfile src/sendfile
