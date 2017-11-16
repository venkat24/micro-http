/**
 * @file server.c
 * A minimal HTTP/1.1 static file server. Built in accordance with :
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

// Define a standard CRLF line ending
#define EOL "\r\n"
#define EOLSIZE 2

// Define a standard buffer allocation size
#define BUFSIZE 1024

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
 * Helper function to manually populate the request buffer. Reads from the
 * stream until the EOL character is reached, and returns the new populated
 * buffer. EOL character is a CRLF
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
			if (eol_characters_matched == EOLSIZE) {
				*(tempbuffer+1-EOLSIZE) = '\0';
				return (strlen(buffer));
			}
		}
		else
			eol_characters_matched = 0;
		tempbuffer++;
	}
	error("Request format invalid: Unterminated request stream");
	return 1;
}

/**
 * A helper function to copy contents of one buffer into another
 * An alternative to strcpy()
 *
 * @param[in]       srcbuffer
 * @param[in]       len        length of the buffer
 */
char* bufcpy(char* srcbuffer, size_t len) {
	char* p = malloc(len * sizeof(char));
	memcpy(p, srcbuffer, len * sizeof(char));
	return p;
}

/**
 * Function to parse the HTTP request headers
 *
 * @param[in]     request
 * @return		  number of headers
 */
int parse_headers(char* request) {
	// Create a temporary buffer to copy the request into
	char *temprequest = malloc(BUFSIZE);
	temprequest = bufcpy(request, BUFSIZE);

	printf("--- REQUEST RECEIVED --- \n");

	printf("\n%s", temprequest);

	// Holds the main header parameters
	// 0 - Request Type
	// 1 - Resource requested
	// 2 - Protocol (It's usually HTTP/1.1)
	// The _save_buffer variables hold the result or the strtok split so that
	// it can be accessed again
	char* request_line[3];
	char* request_line_save_buffer;

	// Read the first line of the input, and split it into it's components
	// The format for the first request line as per RFC 7230 is -
	// [method] [request-target] [HTTP-version] CRLF
	request_line[0] = strtok_r(temprequest, " \t\n", &request_line_save_buffer);
	request_line[1] = strtok_r(NULL, " \t", &request_line_save_buffer);
	request_line[2] = strtok_r(NULL, " \t\n", &request_line_save_buffer);
	printf("Request Type     - %s\n", request_line[0]);
	printf("Request Resource - %s\n", request_line[1]);
	printf("Request Protocol - %s\n", request_line[2]);
	printf("\nHEADERS : \n");

	// Iterate through the remaining request headers.
	// Buffers that store the current header being read
	char* current_header;
	char* current_header_save_buffer;
	// Buffers that store the current header token being read
	char* current_token;
	char* current_token_save_buffer;

	// Count the number of headers in the request
	int header_count = 0;

	// Copy the request into a temp for manipulation
	temprequest = bufcpy(request, BUFSIZE);

	// Iterate over the headers
	for (int i=0;; temprequest = NULL, ++i) {
		// Read the current line of input
		current_header =
			strtok_r(temprequest, "\n", &current_header_save_buffer);

		// Skip the first line. We've already parsed that
		// TODO: This is kinda ugly. Fix it.
		if (i == 0) {
			continue;
		}

		// Split the header from the key:value pair
		// The format of a header as per RFC 7230 is -
		// [field-name]: [field-value]
		// Split the header key
		current_token
			= strtok_r(current_header, ":", &current_token_save_buffer);

		// If the headers are over, stop
		// Check for a NULL value or a trailing CRLF
		// HTTP Requests typically drop a CRLF (optional by RFC 7230)
		if (current_token == NULL 
			|| (strlen(current_token) == 1 && *current_token == EOL[0]))
		{
			printf("--- REQUEST COMPLETE --- \n\n");
			break;
		}
		printf("Header : %s\n", current_token);

		// Split the header value
		current_token = strtok_r(NULL, "\n", &current_token_save_buffer);
		current_token_save_buffer = NULL;

		// If the header value has a leading space, remove it
		if(*current_token == ' ') {
			current_token++;
		}

		printf("Value  : %s\n\n", current_token);
	}

	return header_count;
}

/**
 * HTTP Handler
 * Reads input bytes from the request socket and responds
 *
 * @param[in]     sockfd
 */
int handler(int sockfd) {
	// Send a "Hello World" HTTP response to the client
	char *request = malloc(BUFSIZE);
	char *response = malloc(BUFSIZE);

	/*bufrecv(sockfd, request);*/
	recv(sockfd, request, BUFSIZE, 0);

	parse_headers(request);

	write(sockfd, "HTTP/1.1 200 OK\n", 16);
	write(sockfd, "Content-length: 46\n", 19);
	write(sockfd, "Content-Type: text/html\n\n", 25);
	strcpy(response, "<html><body><H1>Hello world</H1></body></html>\r\n");
	write(sockfd, response, strlen(response));
	return 1;
}

/**
 * Opens a socket on the HTTP listen port and listens for connections
 */
int main(int argc, char* argv[]) {
	// If the user does not specify a port, point out application usage
	if (argc < 2) {
		fprintf(stderr, "usage : server [port]");
		return 1;
	}

	// Create a socket
	int sockfd, respsockfd;
	socklen_t addrlen;
	struct sockaddr_in address;

	// Check for successful socket initialization
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
		printf("Socket creation failed");
	}

	// Set binding parameters
	int port = atoi(argv[1]);
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
		if ((respsockfd
			 = accept(sockfd, (struct sockaddr *) &address, &addrlen)) < 0)
		{
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
