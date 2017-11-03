/**
 * @file server.c
 * A minimal HTTP/1.1 server in C
 */

#include<netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

/**
 * Function to parse the HTTP request headers
 *
 * @param[in]     buffer_in
 */
void parse_headers(char* buffer_in) {
	// TODO: Currently only prints lines
	char *token = NULL;
	token = strtok(buffer_in, "\n");
	while (token) {
		printf("Current token: %s\n", token);
		token = strtok(NULL, "\n");
	}
	return;
}

/**
 * Opens a socket on the HTTP listien port and listens for connections
 */
int main(int argc, char* argv[]) {
	// Create a socket
	int create_socket, new_socket;
	socklen_t addrlen;
	int bufsize = 1024;
	char *buffer = malloc(bufsize);
	struct sockaddr_in address;

	// Check for successful socket initialization
	if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
		printf("Socket creation failed");
	}

	if (argc < 2) {
		fprintf(stderr, "usage : server [port]");
		return 1;
	}

	// Set binding parameters
	long port = strtol(argv[1], NULL, 10);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	// Bind socket to address and port
	if (bind(create_socket, (struct sockaddr *) &address, sizeof(address)) == 0){
		printf("Server started! Listening on port %d\n", port);
	}

	// Begin listen loop
	while (1) {
		// Check for incoming client connections
		if (listen(create_socket, 10) < 0) {
			perror("server: listen");
			exit(1);
		}

		if ((new_socket = accept(create_socket, (struct sockaddr *) &address, &addrlen)) < 0) {
			perror("server: accept");
			exit(1);
		}

		// Check for client connection establishment
		if (new_socket <= 0){
			printf("The Client could not be connected");
		}

		// Send a "Hello World" HTTP response to the client
		recv(new_socket, buffer, bufsize, 0);
		parse_headers(buffer);
		write(new_socket, "HTTP/1.1 200 OK\n", 16);
		write(new_socket, "Content-length: 46\n", 19);
		write(new_socket, "Content-Type: text/html\n\n", 25);
		write(new_socket, "<html><body><H1>Hello world</H1></body></html>",46);
		close(new_socket);
	}
	close(create_socket);
	return 0;
}
