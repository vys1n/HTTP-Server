#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
    lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

char * extract_between(const char *str, const char *p1, const char *p2) {
    const char *i1 = strstr(str, p1);
    if(i1 != NULL) {
        const size_t pl1 = strlen(p1);
        const char *i2 = strstr(i1 + pl1, p2);
        if(p2 != NULL) {
            /* Found both markers, extract text. */
            const size_t mlen = i2 - (i1 + pl1);
            char *ret = malloc(mlen + 1);
            if(ret != NULL) {
                memcpy(ret, i1 + pl1, mlen);
                ret[mlen] = '\0';
                return ret;
            } 
        }
    }

    return "extract_between function failed\n";
}

void formatString(char *buffer, const char *format, ...) {
   va_list args;
   va_start(args, format);
   vsprintf(buffer, format, args);
   va_end(args);
}

void * handle_client(void *arg){
    int client_fd = *((int *)arg);

    // declare a buffer
    char buffer[4096];

    // reading the size of the request
    int bytes_read = read(client_fd, buffer, 4096);

    // variable for server's response to a HTTP Request
    char *response;

    if (startsWith("GET / HTTP/1.1", buffer)) {
        response = "HTTP/1.1 200 OK\r\n\r\n";
    } 

    else if (startsWith("GET /echo/", buffer)) {
        char *text = extract_between(buffer, "GET /echo/", " HTTP/1.1");

        char message[4096];
        formatString(message, "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %lu\r\n\r\n%s\r\n\r\n", 
                     strlen(text), text);

        response = message;
    } 

    else if (startsWith("GET /user-agent HTTP/1.1", buffer)) {
        char *text = extract_between(buffer, "User-Agent: ", "\r\n");

        char message[4096];
        formatString(message, "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %lu\r\n\r\n%s\r\n\r\n", 
                     strlen(text), text);

        response = message;
    } 

    else if (startsWith("GET /files/", buffer)) {
        char *text = extract_between(buffer, "GET /files/", " HTTP/1.1");
        // printf("file name: %s\n", text);

        char file_path[4096];
        // formatString(file_path, "/tmp/%s", text);
        formatString(file_path, "/tmp/data/codecrafters.io/http-server-tester/%s", text);
        // printf("file path: %s\n", file_path);

        int result = access(file_path, F_OK);
        if (result == -1) {
            // printf("FILE: access function failed\n");
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
        } 

        else {
            FILE *file;
            char data[4096];
            file = fopen(file_path, "r");

            if (file == NULL) {
                printf("file not there\n");
            } 

            else {
                // printf("file is open\n");
                while (fgets(data,4096,file) != NULL) {
                    // printf("file content: %s\n", data);
                }
            }

            char message[4096];
            formatString(message, "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/octet-stream\r\n"
                         "Content-Length:%lu\r\n\r\n%s\r\n\r\n", 
                         strlen(data), data);

            response = message;
        }
    } 

    else if (startsWith("POST /files/", buffer)) {
        char *text = extract_between(buffer, "POST /files/", " HTTP/1.1");
        // printf("file name: %s\n", text);

        char file_path[4096];
        // formatString(file_path, "/tmp/%s", text);
        formatString(file_path, "/tmp/data/codecrafters.io/http-server-tester/%s", text);
        // printf("file path: %s\n", file_path);

        char *content_lenght_str = extract_between(buffer, "Content-Length: ", "\r\n");
        int content_length = atoi(content_lenght_str);
        // printf("content length (int): %d\n", content_length);

        buffer[bytes_read] = '\0';
        char *request_body = strstr(buffer, "\r\n\r\n");
        if (request_body != NULL) {
            request_body += 4;
            // printf("Headers: \n%.*s\n", (int)(request_body - buffer - 4), buffer);
            // printf("Body: \n%s\n", request_body);
        } else {
            printf("request_body strstr() error");
        }

        FILE *file;
        file = fopen(file_path, "w");

        if (file == NULL) {
            printf("file not there\n");
        }

        else {
            // printf("file created successfully\n");

            fputs(request_body, file);
            // fputs("\n", file);

            // printf("data written in the file: %s\n", request_body);
            // printf("file content length from req_body: %lu\n", strlen(request_body));
            // printf("file content length from content length: %d\n", content_length);

            fclose(file);
            // printf("file is now closed\n");

            response = "HTTP/1.1 201 Created\r\n\r\n";
        }
    }

    else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    send(client_fd, response, strlen(response), 0);

    return NULL;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	int server_fd;
    socklen_t client_addr_len;
	struct sockaddr_in client_addr;

	// create a Socket
    // server_fd is my socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	// config socket
    struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};

	// bind socket to port
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	// listen for connections
    int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}


    while (1) {

        printf("Waiting for a client to connect...\n");
        client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        printf("Client connected\n");

        // thread (concurrent connections)
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)&client_fd);
        pthread_detach(thread_id);

    }

    close(server_fd);

    return 0;
}
