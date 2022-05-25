/*
Name: Hasne Hossain
ID: 1102602

Skeleteton code taken from practical

// A simple server in the internet domain using TCP
// The port number is passed as an argument
// To compile: gcc server.c -o server
// Reference: Beej's networking guide, man pages
*/

#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// added by Hasne
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <pthread.h>

#define IMPLEMENTS_IPV6

/*  Status Codes    */
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define OK 200

/*  Types   */
#define HTML_TYPE "html"
#define HTML_MIME "text/html"
#define CSS_TYPE "css"
#define CSS_MIME "text/css"
#define JS_TYPE "js"
#define JS_MIME "text/javascript"
#define JPG_TYPE "jpg"
#define JPG_MIME "image/jpeg"

/*  Responses   */
#define GET "GET"
#define NOT_FOUND_RESPONSE "HTTP/1.0 404 Not Found \r\n\r\n"
#define BAD_REQUEST_RESPONSE "HTTP/1.0 400 Bad Request\r\n\r\n"
#define RESPONSE_HEADER_HTML "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"
#define RESPONSE_HEADER_CSS "HTTP/1.0 200 OK\r\nContent-Type: text/css\r\n\r\n"
#define RESPONSE_HEADER_JS "HTTP/1.0 200 OK\r\nContent-Type: text/javascript\r\n\r\n"
#define RESPONSE_HEADER_JPG "HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\n\r\n"
#define RESPONSE_HEADER_OTHER "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n"

typedef struct
{
    //char* buffer;
    char *web_root_dir;
    int socket_file_desc;
    int thread;
} thread_input_t;

void print_response_header(char *final_token, int newsockfd);
int bad_request_check(char *primitive, int newsockfd);
int check_consecutive_dots(char *tmp_path, int newsockfd);
int check_directory(char *final_file_path, int newsockfd);
void file_open_attempt(char *final_file_path, int newsockfd, char *buffer);
void *connection_handler(void *input);

int main(int argc, char **argv)
{
    int sockfd, re, s;
    //char buffer[2100];
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;

    if (argc < 4)
    {
        fprintf(stderr, "ERROR, no protocol/port/web root path provided\n");
        exit(EXIT_FAILURE);
    }

    //protocol, port num and web root parsing
    int protocolno = atoi(argv[1]);
    //int portno = atoi(argv[2]);
    char *web_root_dir = argv[3];
    //printf("web root = %s\nport num=%d\n", web_root_dir, portno);

    int consecutive_dots = 0;
    int single_dot = 0;

    /*check presence of file extension and more than 2 consecutive dots*/
    for (int i = 0; i < strlen(web_root_dir); i++)
    { //strtok is the other option

        //FOR TESTING
        /*if (web_root_dir[i] == '/')
        {
            printf("i=%c\n", web_root_dir[i]);
        } */

        if (web_root_dir[i] == '.')
        {
            single_dot = 1;
            consecutive_dots += 1;
            if (consecutive_dots == 2)
            {
                single_dot = 0;
                //printf("eee....single dot is 0!!\n");
            }
            else if (consecutive_dots > 2)
            {
                fprintf(stderr, "wrong web root given 1\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                continue;
            }
        }
        else
        {
            if ((single_dot == 1) && (web_root_dir[i] != '/'))
            {
                //printf("char= %c\n", web_root_dir[i]);
                fprintf(stderr, "wrong web root given111");
                exit(EXIT_FAILURE);
            }
            consecutive_dots = 0;
            single_dot = 0;
            continue;
        }
    }

    /*checking if directory exists*/
    DIR *dir = opendir(web_root_dir);
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        fprintf(stderr, "wrong web root given 2");
        exit(EXIT_FAILURE);
    }

    // Create address we're going to listen on (with given port number)
    memset(&hints, 0, sizeof hints);
    //hints.ai_family = AF_INET;       // IPv4...use val from argv[1]
    if (protocolno == 4)
    {
        hints.ai_family = AF_INET;
    }
    else if (protocolno == 6)
    {
        hints.ai_family = AF_INET6;
    }
    else
    {
        fprintf(stderr, "Protocol number has to be 4 or 6");
        exit(EXIT_FAILURE);
    }
    //hints.ai_family = AF_INET;       // IPv4...use val from argv[1]
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
    // node (NULL means any interface), service (port), hints, res

    s = getaddrinfo(NULL, argv[2], &hints, &res); //port will now be in argv[2]
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Reuse port if possible
    re = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // Bind address to the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);

    // Listen on socket - means we're ready to accept connections,
    // incoming connection requests will be queued, man 3 listen
    //if (listen(sockfd, 5) < 0) {
    if (listen(sockfd, SOMAXCONN) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept a connection - blocks until a connection is ready to be accepted
    // Get back a new file descriptor to communicate on
    client_addr_size = sizeof client_addr;
    pthread_t thread_id[5];
    int i = -1;
    thread_input_t input[5];
    int newsockfd[5];
    while (1)
    {
        i += 1;
        if (i == 5)
        {
            i = 0;
        }
        newsockfd[i] =
            accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (newsockfd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        //pthread_t thread_id;
        //thread_input_t input;

        input[i].web_root_dir = (char *)malloc(sizeof(char) * strlen(web_root_dir) + 1);
        //printf("web root dir in loop= %s , lenght=%ld\n", web_root_dir, strlen(web_root_dir));

        strcpy(input[i].web_root_dir, web_root_dir);
        size_t len = strlen(input[i].web_root_dir);
        input[i].web_root_dir[len] = '\0';
        //printf("web root in struct= %s , len=%ld\n", input[i].web_root_dir, strlen(input[i].web_root_dir));

        input[i].thread = thread_id[i];
        input[i].socket_file_desc = newsockfd[i];

        // Create a thread to handle the connection.
        pthread_create(&(thread_id[i]), NULL, connection_handler, (void *)&input[i]);
        //pthread_join(thread_id, NULL);
        //pthread_detach(thread_id);

        /*..............................................FINAL MAIN CONNECTION HANDLER..................................*/
    }
    close(sockfd);
    //close(newsockfd);
    return 0;
}

/*Printing required response header*/
void print_response_header(char *final_token, int newsockfd)
{
    char *res1;
    if ((strcmp(final_token, HTML_TYPE) == 0))
    {
        //printf("EQUAL\n");
        res1 = RESPONSE_HEADER_HTML;
        //printf("%s\n", res1);
    }

    else if ((strcmp(final_token, CSS_TYPE) == 0))
    {
        res1 = RESPONSE_HEADER_CSS;
        //printf("%s\n", res1);
    }
    else if ((strcmp(final_token, JS_TYPE) == 0))
    {
        //printf("EQUAL\n");
        res1 = RESPONSE_HEADER_JS;
        //printf("%s\n", res1);
    }
    else if ((strcmp(final_token, JPG_TYPE) == 0))
    {
        //printf("EQUAL\n");
        res1 = RESPONSE_HEADER_JPG;
        //printf("%s\n", res1);
    }
    else
    {
        //printf("EQUAL\n");
        res1 = RESPONSE_HEADER_OTHER;
        //printf("%s\n", res1);
    }
    //int n = 0;
    //n = write(newsockfd, res1, strlen(res1));
    write(newsockfd, res1, strlen(res1));
    //printf("header length=%d\n", n);
}

/*check for 400 bad request*/
int bad_request_check(char *primitive, int newsockfd)
{
    int response_req_code = 0;
    if (strcmp((primitive), "GET") != 0)
    {
        response_req_code = 400;

        //req_file_path = NULL;
        //printf("BAD REQ RESPONSE TO BE SENT\n");
        //printf("HTTP/1.0 400\n");

        char *res_bad = "HTTP/1.0 400 Bad Request\r\n\r\n";
        write(newsockfd, res_bad, strlen(res_bad));

        //n = write(newsockfd, "HTTP/1.0 400\r\n", 18);
        //int n = write(newsockfd, res_bad, strlen(res_bad));
        //printf("bad header length=%d\n", n);

        close(newsockfd);
    }
    return response_req_code;
}

/*check for consecutive dots in path*/
int check_consecutive_dots(char *tmp_path, int newsockfd)
{
    int req_consecutive_dots = 0;
    int response_req_code = 0;
    for (int i = 0; i < strlen(tmp_path); i++)
    {
        if (tmp_path[i] == '.')
        {
            //req_single_dot = 1;
            req_consecutive_dots += 1;
            //printf("req_cons_dots= %d\n", req_consecutive_dots);
            if (req_consecutive_dots == 2)
            {
                //single_dot = 0;
                //printf("404: not founddddddd\n");
                //return 404;

                response_req_code = 404;

                //req_file_path = NULL;
                //printf("NOT FOUND RESPONSE TO BE SENT\n");
                //printf("HTTP/1.0 404\n");
                //char *res1 = "HTTP/1.0 404 Not Found\r\n\r\n";

                char *res1 = NOT_FOUND_RESPONSE;
                write(newsockfd, res1, strlen(res1));

                //n = write(newsockfd, "HTTP/1.0 404\r\n", 18);
                //int n = write(newsockfd, res1, strlen(res1));
                //printf("Not Found due to .. header length=%d\n", n);

                close(newsockfd);
                break;
            }
            continue;
        }
        continue;
    }
    return response_req_code;
}

/*check for directory existence*/
int check_directory(char *final_file_path, int newsockfd)
{
    int response_req_code = 0;
    DIR *dir2 = opendir(final_file_path);
    if (dir2)
    {
        /* Directory exists. */
        closedir(dir2);
        //printf("404: it's directory, so file not found\n");
        response_req_code = 404;
        
        //req_file_path = NULL;
        //printf("NOT FOUND RESPONSE TO BE SENT\n");
        //printf("HTTP/1.0 404\n");

        char *res1 = "HTTP/1.0 404 Not Found \r\n\r\n";
        
        //n = write(newsockfd, "HTTP/1.0 404\r\n", 18);
        //int n = write(newsockfd, res1, strlen(res1));
        //printf("Not Found as it's a directory header length=%d\n", n);

        write(newsockfd, res1, strlen(res1));
        
        close(newsockfd);
    }
    return response_req_code;
}

//malloc involved, make sure to free
/*attempt to open file*/
void file_open_attempt(char *final_file_path, int newsockfd, char *buffer)
{
    char *req_file_path;
    //int response_req_code; //initialise separately in the if-else blocks instead
    //printf("response req code= %d\n", response_req_code);

    FILE *file;
    file = fopen(final_file_path, "r");
    if (file == NULL)
    {
        //printf("404: not found\n");
        //return 404;
        //int response_req_code = 404;
        req_file_path = NULL;
        //printf("NOT FOUND RESPONSE TO BE SENT\n");
        //printf("HTTP/1.0 404\n");

        char *res1 = "HTTP/1.0 404 Not Found\r\n\r\n";

        //n = write(newsockfd, "HTTP/1.0 404\r\n", 18);
        //int n = write(newsockfd, res1, strlen(res1));
        //printf("Not Found file header length=%d\n", n);

        write(newsockfd, res1, strlen(res1));
        close(newsockfd);
    }
    else
    {
        //printf("200: file found!\n");
        //return 200;
        //int response_req_code = 200;
        req_file_path = final_file_path; //will need to malloc if we make it a separate func
        //printf("GOOD RESPONSE TO BE SENT\n");
        //printf("HTTP/1.0 200 OK\nContent-Type:\n");

        /*mime handling.....................*/
        //int consecutive_dots = ".."; //for future use
        char *stopper = ".";
        char *token;
        char *final_token;
        //char *copy_req_file_path = (char *)malloc(sizeof(char) * strlen(buffer)*2);
        char *copy_req_file_path = (char *)malloc(sizeof(char) * strlen(req_file_path) + 2);
        assert(copy_req_file_path);
        strcpy(copy_req_file_path, req_file_path);

        /* get the first token */
        token = strtok(copy_req_file_path, stopper);
        //printf("token= %s\n", token);

        /* walk through other tokens */
        while (token != NULL)
        {
            //printf(" token= %s\n", token);
            final_token = token;
            token = strtok(NULL, stopper);
        }
        //printf("req file path, token = %s , %s\n", req_file_path, final_token);
        //printf("final token=%s\n", final_token);

        print_response_header(final_token, newsockfd);

        FILE *file2 = fopen(req_file_path, "rb"); //if file is image type
        assert(file2);

        ssize_t n2 = 0;
        int file2fd = fileno(file2);
        //sendfile(newsockfd, file2fd, NULL, 4);
        while ((n2 = sendfile(newsockfd, file2fd, NULL, 100)) > 0)
        {
            printf("to be continued\n");
        }
        close(newsockfd);
    }
}


/*central connection handler*/
void *connection_handler(void *input)
{

    thread_input_t *vars = (thread_input_t *)input;
    printf("web root in handler= %s , len=%ld\n", (*vars).web_root_dir, strlen((*vars).web_root_dir));
    printf("web root in handler after fix= %s\n", (*vars).web_root_dir);
    char *web_root_dir = (*vars).web_root_dir;
    int newsockfd = (*vars).socket_file_desc;

    char buffer[2100];
    int n = read(newsockfd, buffer, 2099); // n is number of characters read
    if (n < 0)
    {
        perror("error in reading from socket");
        exit(EXIT_FAILURE);
    }
    // Null-terminate string
    buffer[n] = '\0';
    printf("buffer = %s\n", buffer);

    char *tmp_path = (char *)malloc(sizeof(char) * strlen(buffer));
    char primitive[4];
    sscanf(buffer, "%s %s", primitive, tmp_path);
    printf("primitive=%s, tmp_path=%s\n", primitive, tmp_path);

    //final path
    //char* final_file_path = (char*) malloc(sizeof(char)* strlen(buffer)*2);
    char *final_file_path = (char *)malloc(sizeof(char) * strlen(buffer) * 2);
    assert(final_file_path);
    sprintf(final_file_path, "%s%s", web_root_dir, tmp_path);
    printf("final file path=%s\n", final_file_path);

    int response_req_code = 0; //bad=400, not_found=404, ok=200
    //char* req_file_path; //learn where to use it

    //wrong type of request sent
    response_req_code = bad_request_check(primitive, newsockfd);

    printf("tmp path in loop = %s\n", tmp_path);
    if (response_req_code == 0)
    {
        response_req_code = check_consecutive_dots(tmp_path, newsockfd);
    }

    if (response_req_code == 0)
    {
        /*check if requested path is a directory*/
        response_req_code = check_directory(final_file_path, newsockfd);
    }

    if (response_req_code == 0)
    {
        //checking the path
        //var to use -> final_file_path
        /*................................................file open attempt............................*/
        file_open_attempt(final_file_path, newsockfd, buffer);
    }
    close(newsockfd);
    pthread_exit(NULL);
}

