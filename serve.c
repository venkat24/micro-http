/**
 * @file serve.c
 * A minimal HTTP/1.1 static file server.
 */

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

// Define a standard CRLF line ending
#define EOL "\r\n"
#define EOLSIZE 2

// Define a standard buffer allocation size
#define BUFSIZE 1024

// Define a large buffer allocation size
#define BIGBUFSIZE 1024

// Define a number of headers
#define HEADERCOUNT 50

// Fix the locaation from which the files are served
char WEBROOT[256];

/**
 * Structure to hold a header 
 */
struct header_frame {
	char field[BUFSIZE];
	char value[BUFSIZE];
};

/**
 * Structure to hold a request
 */
struct request_frame {
	// Request method - GET, POST, etc.
	char* method;
	// Location of the requested resource, like /home/index.html
	char* resource;
	// Protocol (Usually HTTP/1.1)
	char* protocol;
	// Body of the request
	char* body;
	// Number of headers
	int header_count;
	// A list of headers
	struct header_frame headers[HEADERCOUNT];
};

/**
 * Structure to hold a response
 */
struct response_frame {
	// Protocol (Usally HTTP/1.1)
	char* protocol;
	// Status code - 200, 400, etc
	int status_code;
	// Response message - OK, NOT FOUND, etc
	char* status_message;
	// Number of headers
	int header_count;
	// A list of response headers
	struct header_frame headers[HEADERCOUNT];
};

/**
 * Structure of mapping from an extension to a MIME type
 */
typedef struct {
    char *ext;
    char *mimetype;
} extn;

/**
 * A list of common file extensions and their MIME types
 * TODO: Extend this list
 */
extn extensions[] ={
	{"aiff", "audio/x-aiff"},
	{"avi", "video/avi"},
	{"bin", "application/octet-stream"},
	{"bmp", "image/bmp"},
	{"css", "text/css"},
	{"c", "text/x-c"},
	{"doc", "application/msword"},
	{"gif", "image/gif" },
	{"gz",  "image/gz"  },
	{"htmls", "text/html"},
	{"html","text/html" },
	{"html", "text/html"},
	{"ico", "image/ico"},
	{"jpeg","image/jpeg"},
	{"jpg", "image/jpg" },
	{"js", "application/x-javascript"},
	{"mp3", "audio/mpeg3"},
	{"mpeg", "video/mpeg"},
	{"mpg", "video/mpeg"},
	{"md", "text/markdown"},
	{"pdf","application/pdf"},
	{"php", "text/html" },
	{"png", "image/png" },
	{"png", "image/png"},
	{"rar","application/octet-stream"},
	{"tar", "image/tar" },
	{"tiff", "image/tiff"},
	{"txt", "text/plain" },
	{"xml", "application/xml"},
	{"zip", "application/zip"}
};

/**
 * Helper function to throw an error
 *
 * @param[in]     error_message    error message to print before exit
 */
void error(const char *msg) {
	perror(msg);
	exit(1);
}

/**
 * Helper function to check if a given path points to a filename of a
 * directory.
 *
 * @param[in]     path      path to determine of file or directory
 */
int is_regular_file(const char *path) {
	struct stat path_stat;
	stat(path, &path_stat);
	return S_ISREG(path_stat.st_mode);
}

/**
 * A helper function to copy contents of one buffer into another
 * An alternative to strcpy()
 *
 * @param[in]       srcbuffer  buffer to create copy of
 * @param[in]       len        length of the buffer
 */
char* bufcpy(char* srcbuffer, size_t len) {
	char* p = malloc(len * sizeof(char));
	memcpy(p, srcbuffer, len * sizeof(char));
	return p;
}

/**
 * Helper function to get the extension, given the filename
 */
const char *get_filename_ext(const char *filename) {
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return "";
	return dot + 1;
}


/**
 * Function to parse the HTTP request headers and body
 * Returns 1 if parsing successful
 *
 * @param[in]        request                 request bytes
 * @param[out]       request_frame struct    output parsed request
 */
int parse_request(char* requestbuf, struct request_frame *request) {
	// Create a temporary buffer to copy the request into
	char *temprequest = malloc(BUFSIZE);
	temprequest = bufcpy(requestbuf, BUFSIZE);

	// Read the first line of the input, and split it into it's components
	// The format for the first request line as per RFC 7230 is -
	// [method] [request-target] [HTTP-version] CRLF
	char* request_save_buffer;
	request->method   = strtok_r(temprequest, " \t\n", &request_save_buffer);
	request->resource = strtok_r(NULL, " \t", &request_save_buffer);
	request->protocol = strtok_r(NULL, " \t\n", &request_save_buffer);

	// Iterate through the remaining request headers.
	// Buffers that store the current header being read
	char* current_header;
	char* current_header_save_buffer;
	// Buffers that store the current header token being read
	char* current_token;
	char* current_token_save_buffer;

	// Store the headers as they're parsed
	struct header_frame tempheader;
	int header_count = 0;

	// Flag to check if parsing headers is complete
	int headers_complete_flag = 0;

	// Copy the request into a temp for manipulation
	temprequest = bufcpy(requestbuf, BUFSIZE);

	// Iterate over the headers
	for (;; temprequest = NULL) {
		// If all the headers have been parsed, only the request body is left
		if (headers_complete_flag) {
			request->body = current_header_save_buffer;
			break;
		}

		// Read the current line of input
		current_header =
			strtok_r(temprequest, "\n", &current_header_save_buffer);

		// Skip the first line. We've already parsed that
		// TODO: This is kinda ugly. Fix it.
		if (header_count == 0) {
			header_count++;
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
			headers_complete_flag = 1;
			continue;
		}
		strcpy(tempheader.field, current_token);

		// Split the header value
		current_token = strtok_r(NULL, "\n", &current_token_save_buffer);
		current_token_save_buffer = NULL;

		// If the header value has a leading space, remove it
		if(*current_token == ' ') {
			current_token++;
		}

		strcpy(tempheader.value, current_token);
		request->headers[header_count++] = tempheader;
	}

	request->header_count = header_count;

	free(temprequest);
	return 1;
}

/**
 * Takes a parsed request struct and prints it
 *
 * @param[in]   request_frame
 */
void print_request_verbose(const struct request_frame *request) {
	// Pretty print the parsed request
	printf("--- REQUEST RECEIVED --- \n");
	printf("\nRequest Method   - %s", request->method);
	printf("\nRequest Resource - %s", request->resource);
	printf("\nRequest Protocol - %s", request->protocol);
	printf("\n\nHEADERS : \n");

	for(int i = 1; i < request->header_count; ++i) {
		printf("Header : %s\n", request->headers[i].field);
		printf("Value  : %s\n\n", request->headers[i].value);
	}

	printf("Body : %s\n", request->body);
	printf("--- REQUEST COMPLETE ---\n\n");
}

/**
 * Takes a parsed request struct and prints it (sparse)
 *
 * @param[in]   request_frame
 */
void print_request_sparse(const struct request_frame *request) {
	printf("\n%s %s %s",
	    request->method, request->resource, request->protocol);
}

/**
 * Takes a parsed request and generates the response
 *
 * @param[in]    request_frame    the parsed request struct
 * @param[out]   response_frame   output parsed response struct
 */
void response_generator(
	const struct request_frame* req,
	struct response_frame* res
) {
	// Set the response protocol
	res->protocol = "HTTP/1.1";

	// Initialize the number of headers
	res->header_count = 0;

	// Set the standard response unless otherwise
	res->status_code = 200;
	res->status_message = "OK";

	// Check the request protocol field
	if( ! (strcmp(req->protocol,"HTTP/1.1")
		|| strcmp(req->protocol,"HTTP/1.0")) ) {
		res->status_code = 400;
		strcpy(res->status_message, "Bad Request");
	}

	// Determine the absolute path to the requested resource
	char* resource_path = req->resource;
	char* full_resource_path = malloc(BUFSIZE);
	strcpy(full_resource_path, WEBROOT);
	strcat(full_resource_path, resource_path);

	// Check if the requested resource is a file, or a directory
	if (is_regular_file(full_resource_path)) {
		// If it's a file, read the contents and serve the file

		// Find the file size, to set the Content-Lenth field
		FILE *fresource = fopen(full_resource_path, "rb");
		fseek(fresource, 0L, SEEK_END);
		int file_size = ftell(fresource);
		fclose(fresource);

		// Find the extension of the file, and get it's MIME Type
		// TODO: Replace this with a hashtable for improved performance
		const char* this_extension;
		this_extension = get_filename_ext(full_resource_path);

		// Set the Content-Length header
		char content_length[20];
		sprintf(content_length, "%d", file_size);
		// ^ Converting the integer into a string (hacky?)
		strcpy(res->headers[res->header_count].field, "Content-Length");
		strcpy(res->headers[res->header_count].value, content_length);
		res->header_count++;

		// Set the content type header. Find this from the extension.
		// Iterate through the extensions list and break out when it's been
		// matched. Copy over the corresponding MIME type
		char mimetype[40];
		int mime_set = 0;
		for (int i = 0; i < sizeof(extensions)/sizeof(extn); ++i) {
			if (!strcmp(extensions[i].ext, this_extension)) {
				mime_set = 1;
				strcpy(mimetype, extensions[i].mimetype);
				break;
			}
		}
		if (! mime_set) {
			strcpy(mimetype, "application/octet-stream");
		}
		strcpy(res->headers[res->header_count].field, "Content-Type");
		strcpy(res->headers[res->header_count].value, mimetype);
		res->header_count++;
	}
	free(full_resource_path);
}

/**
 * Function to send back response through socket
 *
 * param[in]      sockfd           the socket to send the response through
 * param[in]      request_frame    the request information object
 * param[in]      response_frame   the response information object
 */
void send_response(
	int sockfd,
	const struct request_frame* req,
	const struct response_frame* res
) {

	// Set the first status line
	char* header_line = malloc(BUFSIZE);
	sprintf(
		header_line, "%s %d %s\n",
		res->protocol,
		res->status_code,
		res->status_message
	);
	write(sockfd, header_line, strlen(header_line));

	// Write the headers
	// Iterate through the headers, sprintf them into the right format, store
	// them in the temp, and write them to the stream
	char* header = malloc(BUFSIZE);
	for(int i=0; i < res->header_count; ++i ) {
		sprintf(
			header,
			"%s: %s\n",
			res->headers[i].field,
			res->headers[i].value
		);
		write(sockfd, header, strlen(header));
	}

	// Write a newline to separate the headers and the body
	write(sockfd, "\n", 1);

	// Write the response body
	// Determine the absolute path to the requested resource
	char* resource_path = req->resource;
	char* full_resource_path = malloc(BUFSIZE);
	strcpy(full_resource_path, WEBROOT);
	strcat(full_resource_path, resource_path);

	// If it's not a regular file, it's a directory
	// Serve an index page with a file listing
	if (is_regular_file(full_resource_path)) {
		// Allocate and assign the main response body
		// This opens the requested resource and assigns it onto the body buffer
		char response_buffer[BIGBUFSIZE];

		// Render the file
		FILE *fresource = fopen(full_resource_path, "rb");
		int nread = 0;
		// Read the file in chunks, and write them to the stream
		if (fresource) {
			while(nread = fread(response_buffer, 1, BIGBUFSIZE, fresource)){
				write(sockfd, response_buffer, sizeof(response_buffer));
			}
			write(sockfd, EOL, EOLSIZE);
		} else {
			error("FILE INVALID!");
		}
		fclose(fresource);
	}
	else {
		// It's a directory. Display a file listing.
		// Initialize directory check parameters
		DIR *directory;
		struct dirent *dir_struct;
		char current_line[BIGBUFSIZE];
		char* dirtemp = malloc(BIGBUFSIZE);
		char* resource_path_ptr = malloc(BIGBUFSIZE);
		directory = opendir(full_resource_path);

		// Begin rendering the HTML for the file listing
		strcpy(current_line, "<html><body><h1>File Listing</h1><ul>");
		write(sockfd, current_line, strlen(current_line));
		if (directory) {
			resource_path_ptr = resource_path;
			if (!strcmp(resource_path_ptr, "/")) {
				strcpy(resource_path_ptr, "");
			}
			while(*resource_path_ptr == '/') {
				resource_path_ptr++;
			}
			while ((dir_struct = readdir(directory)) != NULL) {
				strcpy(dirtemp, dir_struct->d_name);
				if (!strcmp(dirtemp, ".")
				 || !strcmp(dirtemp, "..")) {
					continue;
				}
				while(*dirtemp == '/') {
					dirtemp++;
				}
				if(!strlen(resource_path_ptr)) {
					sprintf(current_line, "<a href=\"%s/%s\"><li>%s</li></a>", resource_path_ptr, dirtemp, dirtemp);
				}
				else {
					sprintf(current_line, "<a href=\"/%s/%s\"><li>%s</li></a>", resource_path_ptr, dirtemp, dirtemp);

				}
				write(sockfd, current_line, strlen(current_line));
			}
			closedir(directory);
		}
		strcpy(current_line, "</ul></body></html>");
		write(sockfd, current_line, strlen(current_line));
	}

	free(header);
	free(header_line);
	free(full_resource_path);
}

/**
 * HTTP Handler
 * Handles a single request.
 *
 * @param[in]     sockfd     socket to write response to
 */
int handler(int sockfd) {
	// Allocate buffers to read the request
	char *request = malloc(BUFSIZE);
	struct request_frame req;
	struct response_frame res;

	// Read request from socket
	recv(sockfd, request, BUFSIZE, 0);

	// Parse the request and print to stdout
	if (parse_request(request, &req)) {
		// print_request_verbose(&req);
		print_request_sparse(&req);
	}

	// Populate the response struct
	response_generator(&req, &res);
	send_response(sockfd, &req, &res);

	return 1;
}

/**
 * Opens a socket on the HTTP listen port and listens for connections
 */
int main(int argc, char* argv[]) {
	// If the user does not specify a port, point out application usage
	if (argc < 2) {
		fprintf(stderr, "usage : server port [webroot]\n");
		return 1;
	}
	// Useful for debugging. Flushes stdout, without any buffering
	setbuf(stdout, NULL);

	strcpy(WEBROOT, getenv("PWD"));

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

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	// Bind socket to address and port
	if (bind(sockfd, (struct sockaddr *) &address, sizeof(address)) == 0){
		printf("Server started! Listening on port %d\n", port);
	} else {
		error("Error opening connection\n");
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
			error("Could not initialize response process\n");
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
