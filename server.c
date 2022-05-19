// A simple server in the internet domain using TCP
// The port number is passed as an argument
// To compile: gcc server.c -o server
// Reference: Beej's networking guide, man pages

#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h> //hasne
#include <dirent.h> //hasne
#include <errno.h> //hasne

int main(int argc, char** argv) {
	int sockfd, newsockfd, n, re, s;
	char buffer[256];
	struct addrinfo hints, *res;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

	//if (argc < 2) {
	if (argc < 4) {
		fprintf(stderr, "ERROR, no protocol/port/web root path provided\n");
		exit(EXIT_FAILURE);
	}

	//protocol, port num and web root parsing
	int protocolno = atoi(argv[1]);
	int portno = atoi(argv[2]);
	char* web_root_dir = argv[3];
	printf("web root = %s\n", web_root_dir);

	//ask if it makes sense
	int consecutive_dots=0;
	int single_dot = 0;
	for(int i=0; i<strlen(web_root_dir); i++){
		if(web_root_dir[i] == '.'){
			single_dot = 1;
			consecutive_dots += 1;
			if(consecutive_dots == 2){
				single_dot = 0;
			}
			if(consecutive_dots > 2){
				fprintf(stderr, "wrong web root given 1");
				exit(EXIT_FAILURE);	
			}
			else{
				continue;
			}
		}
		else{
			//consecutive_dots -= 1;
			if(single_dot){
				fprintf(stderr, "wrong web root given111");
				exit(EXIT_FAILURE);
			}
			consecutive_dots = 0;
			continue;
		}
	}
	if(single_dot && consecutive_dots==0 ){
			fprintf(stderr, "wrong web root given");
			exit(EXIT_FAILURE);	
	}

	DIR* dir = opendir(web_root_dir);
	if (dir) {
    	/* Directory exists. */
    	closedir(dir);
	}
	else if (ENOENT == errno) {
		fprintf(stderr, "wrong web root given 2");
		exit(EXIT_FAILURE);	
	}
	/*char* server_file_path = (char*) malloc(sizeof(char)* 200); 
	assert(server_file_path);
	sprintf(server_file_path, "%s%s", web_root_dir, "/server.c");
	printf("server_file_path= %s\n", server_file_path);
	if (fopen(server_file_path, "r") == NULL) {
        fprintf(stderr, "wrong web root given");
		exit(EXIT_FAILURE);	
	} */

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
	//hints.ai_family = AF_INET;       // IPv4...use val from argv[1]
	if(protocolno == 4){
		hints.ai_family = AF_INET;
	}
	else if(protocolno == 6){
		hints.ai_family = AF_INET6;
	}
	else{
		fprintf(stderr, "Protocol number has to be 4 or 6");
		exit(EXIT_FAILURE);	
	}
	//hints.ai_family = AF_INET;       // IPv4...use val from argv[1]
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	//s = getaddrinfo(NULL, argv[1], &hints, &res); //port will now be in argv[2]
	s = getaddrinfo(NULL, argv[2], &hints, &res); //port will now be in argv[2]
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	// Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);

	// Listen on socket - means we're ready to accept connections,
	// incoming connection requests will be queued, man 3 listen
	//if (listen(sockfd, 5) < 0) {
	if (listen(sockfd, SOMAXCONN) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
	client_addr_size = sizeof client_addr;
	/*newsockfd =
		accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
	if (newsockfd < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	} */

	while (1){
		newsockfd =
			accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
		if (newsockfd < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		// Read characters from the connection, then process
		n = read(newsockfd, buffer, 255); // n is number of characters read
		if (n < 0) {
			perror("error in reading from socket");
			exit(EXIT_FAILURE);
		}
		// Null-terminate string
		buffer[n] = '\0';

		char* tmp_path = (char*) malloc(sizeof(char) * strlen(buffer));
		char primitive[4];
		sscanf(buffer, "%s %s", primitive, tmp_path);
		printf("primitive=%s, tmp_path=%s\n", primitive, tmp_path);
		//sscanf(buffer, "%s %s %*[A-z0-9/:\n]", primitive, tmp_path);

		//final path
		//char* final_file_path = (char*) malloc(sizeof(char)* strlen(buffer)*2);
		char* final_file_path = (char*) malloc(sizeof(char)* strlen(buffer));
		assert(final_file_path);
		sprintf(final_file_path, "%s%s", web_root_dir, tmp_path);

		int response_req_code; //bad=400, not_found=404, ok=200
		char* req_file_path; //learn where to use it

		//wrong type of request sent
		if(strcmp((primitive), "GET") != 0){
			response_req_code = 400;
			req_file_path = NULL;
			printf("BAD REQ RESPONSE TO BE SENT\n");
			printf("HTTP/1.0 400\n");
			char* res_bad = "HTTP/1.0 400\r\n";
			//n = write(newsockfd, "HTTP/1.0 400\r\n", 18);
			n = write(newsockfd, res_bad, strlen(res_bad));
		}

		int req_single_dot = 0;
		int req_consecutive_dots = 0;
		printf("tmp path in loop = %s\n", tmp_path);
		for(int i=0; i<strlen(tmp_path); i++){
			if(tmp_path[i] == '.'){
				req_single_dot = 1;
				req_consecutive_dots += 1;
				if(req_consecutive_dots == 2){
					//single_dot = 0;
					printf("404: not founddddddd\n");
					//return 404;
					response_req_code = 404;
					req_file_path = NULL;
					printf("NOT FOUND RESPONSE TO BE SENT\n");
					printf("HTTP/1.0 404\n");
					char* res1 = "HTTP/1.0 404\r\n";
					//n = write(newsockfd, "HTTP/1.0 404\r\n", 18);
					n = write(newsockfd, res1, strlen(res1));
					close(newsockfd);
					break;
				}
				continue;
			}
			continue;
		}

		if(response_req_code != 404){
			//checking the path
			//var to use -> final_file_path
			FILE* file;
			file = fopen(final_file_path, "r");
			if(file == NULL){
				printf("404: not found\n");
				//return 404;
				response_req_code = 404;
				req_file_path = NULL;
				printf("NOT FOUND RESPONSE TO BE SENT\n");
				printf("HTTP/1.0 404\n");
				char* res1 = "HTTP/1.0 404\r\n";
				//n = write(newsockfd, "HTTP/1.0 404\r\n", 18);
				n = write(newsockfd, res1, strlen(res1));
			}
			else{
				printf("200: file found!\n");
				//return 200;
				response_req_code = 200;
				req_file_path = final_file_path; //will need to malloc if we make it a separate func
				printf("GOOD RESPONSE TO BE SENT\n");
				printf("HTTP/1.0 200 OK\nContent-Type:\n");
				char* res1 = "HTTP/1.0 200 OK\r\nContent-Type:texttt/html\r\n\r\n";
				//n = write(newsockfd, "HTTP/1.0 200 OK\r\nContent-Type:\n", 18);
				//n = write(newsockfd, res1, strlen(res1));

				/*mime handling.....................*/
				//int consecutive_dots = ".."; //for future use
				char* stopper = ".";
				char* token;
				char* final_token;
				char* copy_req_file_path = (char*) malloc(sizeof(char)* strlen(buffer));
				assert(copy_req_file_path);
				strcpy(copy_req_file_path, req_file_path);

				/* get the first token */
				token = strtok(copy_req_file_path, stopper);
				//printf("token= %s\n", token);
				/* walk through other tokens */

				while( token != NULL ) {
					printf( " token= %s\n", token );
					final_token = token;
					token = strtok(NULL, stopper);
				}
				printf("req file path, token = %s , %s\n", req_file_path, final_token);
				printf("final token=%s\n", final_token);
				if ((strcmp(final_token, "html") == 0)) {
				  printf("EQUAL\n");
     			 	  res1 = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
				  printf( "%s\n", res1);
	    			}
				//printf("testt....%s\n", res1);

				else if ((strcmp(final_token, "css") == 0)) {
     			 	  res1 = "HTTP/1.0 200 OK\r\nContent-Type: text/css\r\n\r\n";
				  printf("%s\n", res1);
	    			}
				else if ((strcmp(final_token, "js") == 0)) {
				   printf("EQUAL\n");
	     			   res1 = "HTTP/1.0 200 OK\r\nContent-Type: text/javascript\r\n\r\n";
				   printf("%s\n", res1);
    				}
				else if ((strcmp(final_token, "jpg") == 0)) {
				   printf("EQUAL\n");
	     			   res1 = "HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\n\r\n";
				   printf("%s\n", res1);
	    			}
				else{
				   printf("EQUAL\n");
     			 	   res1 = "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n";
				   printf("%s\n", res1);
	    			}
				n = write(newsockfd, res1, strlen(res1));
				/*...mime handling end......................*/

				//GOOD TO SEND CONTENT AS WELL
				//FILE* file2 = fopen(req_file_path, "rb"); //if file is image type
				FILE* file2 = fopen(req_file_path, "rb"); //if file is image type
				assert(file2);
				fseek(file2, 0, SEEK_END);
				int len = ftell(file2);
				//changing file pointer back to start to be able to copy it to buffer
				fseek(file2, 0, SEEK_SET);
				unsigned char* buffer2 = (unsigned char*) malloc(len + 1);
				assert(buffer2);

				fread(buffer2, len, sizeof(unsigned char), file2);
				int n2 = write(newsockfd, buffer2, len);
				close(newsockfd);
				//write(newsockfd, buffer2, len);
			}

		}
		//preparing a response
		//char* response;


		// Write message back
		/*printf("Here is the message: %s\n", buffer);
		n = write(newsockfd, "I got your message", 18);
		if (n < 0) {
			perror("write");
			exit(EXIT_FAILURE);
		}*/
	}
	close(sockfd);
	close(newsockfd);
	return 0;
}

