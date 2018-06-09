default:
	g++ server_cc.cpp -o server_cc
	g++ client_cc.cpp -o client_cc

ncc:
	g++ server.cpp -o server
	g++ client.cpp -o client

clean:
	rm -rf server server_cc client client_cc received.data
