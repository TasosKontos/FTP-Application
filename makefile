all:
	g++ -g -std=c++17 -w dataServer.cpp -o dataServer -lpthread
	g++ -g -std=c++17 -w remoteClient.cpp -o remoteClient -lpthread
clean:
	rm remoteClient
	rm dataServer
