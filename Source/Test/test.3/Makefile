CFLAG= -std=c++17 -I ../../../build/release-install-cpp11/include/
LFLAG= -L../../../build/release-install-cpp11/lib  -lmuduo_net -lmuduo_base -pthread -ljsoncpp
all: publish_client subscribe_client server
publish_client: publish_client.cc
	g++ -g  $(CFLAG) $^ -o $@ $(LFLAG)
subscribe_client: subscribe_client.cc
	g++ -g  $(CFLAG) $^ -o $@ $(LFLAG)
server:	server.cc
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)

.PHONY: clean
clean:
	rm -f  server publish_client subscribe_client 