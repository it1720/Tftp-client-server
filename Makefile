build:
	gcc tftp-client.c -o tftp-client
	gcc tftp-server.c -o tftp-server
clean:
	rm tftp-client
	rm tftp-server
