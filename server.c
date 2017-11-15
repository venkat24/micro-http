/**
 * @file server.c
 * A minimal HTTP/1.1 server. Built in accordance with :
 * RFC 7230 (Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing)
 * RFC 7231 (Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content)
 */

#include<netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

// Define a standard CLRF line ending
#define EOL "\r\n"

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
		token = strtok(NULL, "\n");
	}
	return;
}

/**
 * Helper function to throw an error
 *
 * @param[in]     error_message
 */
void error(const char *msg) {
	perror(msg);
	exit(1);
}

/**
 * A helper function to manually populate the request buffer. Reads from the
 * stream until the EOL character is reached, and returns the new populated
 * buffer. EOL character is a CLRF
 *
 * @param[in]       sockfd
 * @param[inout]    buffer
 */
int bufrecv(int sockfd, char *buffer) {
	char *tempbuffer = buffer;
	int eol_characters_matched = 0;
	while (recv(sockfd, tempbuffer, 1, 0) != 0) {
		if (*tempbuffer == EOL[eol_characters_matched]) {
			eol_characters_matched++;
			if (eol_characters_matched == 2) {
				*(tempbuffer-1) = '\0';
				return (strlen(buffer));
			}
		} else {
			eol_characters_matched = 0;
		}
		tempbuffer++;
	}
	error("Request format invalid: Unterminated request stream");
	return 1;
}

/**
 * HTTP Handler
 * Reads input bytes from the request socket and responds
 *
 * @param[in]     sockfd
 */
int handler(int sockfd) {
	// Send a "Hello World" HTTP response to the client
	int bufsize = 1024;

	char *request = malloc(bufsize);
	char *response = malloc(bufsize);
	strcpy(response, "<html><body><H1>Hello world</H1></body></html>");

	bufrecv(sockfd, request);
	printf("%s\n", request);
	parse_headers(request);
	write(sockfd, "HTTP/1.1 200 OK\n", 16);
	write(sockfd, "Content-length: 46\n", 19);
	write(sockfd, "Content-Type: text/html\n\n", 25);
	write(sockfd, response, strlen(response));
	return 1;
}

/**
 * Opens a socket on the HTTP listen port and listens for connections
 */
int main(int argc, char* argv[]) {
	// Create a socket
	int sockfd, respsockfd;
	socklen_t addrlen;
	struct sockaddr_in address;

	// Check for successful socket initialization
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
		printf("Socket creation failed");
	}

	if (argc < 2) {
		fprintf(stderr, "usage : server [port]");
		return 1;
	}

	// Set binding parameters
	long port = strtol(argv[1], NULL, 10);
	char *root;
	root = getenv("PWD");

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	// Bind socket to address and port
	if (bind(sockfd, (struct sockaddr *) &address, sizeof(address)) == 0){
		printf("Server started! Listening on port %d\n", port);
	} else {
		error("Error opening connection");
	}

	// Begin listen loop
	while (1) {
		// Check for incoming client connections
		if (listen(sockfd, 10) < 0) {
			perror("server: listen");
			exit(1);
		}

		if ((respsockfd = accept(sockfd, (struct sockaddr *) &address, &addrlen)) < 0) {
			perror("server: accept");
			exit(1);
		}

		// Spawns a child process which handles the request
		int pid = fork();
		if (pid < 0) {
			error("Could not initialize response process");
		}

		// If child process, close the request socket, and initiate the
		// handler
		if (pid == 0) {
			close(sockfd);
			handler(respsockfd);
			exit(0);
		}
		// If parent process, close the response socket
		else {
			close(respsockfd);
		}
	}
	close(sockfd);
	return 0;
}
