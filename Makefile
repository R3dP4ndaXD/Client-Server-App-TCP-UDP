CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# ID-ul cu care se conecteaza subscriberul la server
ID_subscriber = "1"

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

common.o: common.cpp

# Compileaza server.cpp
server: server.cpp common.o

# Compileaza subscriber.cpp
subscriber: subscriber.cpp common.o

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriberul 	
run_subscriber:
	./subscriber ${ID_subscriber} ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
