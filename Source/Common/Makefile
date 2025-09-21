CFLAG= -std=c++17 -I ../../build/release-install-cpp11/include/
LFLAG= -L../../build/release-install-cpp11/lib  -lmuduo_net -lmuduo_base -pthread -ljsoncpp
all: server client
server: test.cc
	g++ -g  $(CFLAG) $^ -o $@ $(LFLAG)
client: testClient.cc
	g++ -g  $(CFLAG) $^ -o $@ $(LFLAG)

.PHONY: clean
clean:
	rm -f server client