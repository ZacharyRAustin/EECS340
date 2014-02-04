#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <sstream>
#include <iostream>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;
    int sock = 0;
    bool ok = true;
    struct sockaddr_in sa;
    struct hostent * site = NULL;
    char * req = NULL;
    char buf[BUFSIZE + 1];
    //struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
	sock = minet_socket(SOCK_STREAM);
	if(sock == -1)
	{
		fprintf(stderr, "Could not create socket\n");
		exit(-1);
	}

    // Do DNS lookup
    /* Hint: use gethostbyname() */
	site = gethostbyname(server_name);
	if(site == NULL)
	{
		fprintf(stderr, "Could not perform DNS lookup\n");
		minet_close(sock);
		minet_deinit();
		exit(-1);
	}

    /* set address */
	memset((char *) &sa, 0, sizeof(sa));
	sa.sin_family=AF_INET;
	memcpy((char *) &sa.sin_addr, (char *) site->h_addr, site->h_length);
	sa.sin_port = htons(server_port);

    /* connect socket */
	int connect = minet_connect(sock, (sockaddr_in *) &sa);
	if(connect == -1)
	{
		minet_close(sock);
		minet_deinit();
		fprintf(stderr, "Could not connect socket\n");
		exit(-1);
	}  

    /* send request */
	req = (char*) malloc(strlen(server_path) + 100 * sizeof(char)); 

	sprintf(req, "GET %s HTTP/1.0\nHost: %s\nUser-agent: Mozilla/4.0\nConnection: close\nAccept-language: fr\n\r\n\r\n\0", server_path, server_name);

	int write_check = minet_write(sock, req, strlen(req));
	if(write_check < 0)
	{
		minet_close(sock);
		minet_deinit();
		fprintf(stdout, "Could not write request to socket\n");
		fflush(stdout);
		exit(-1);
	}

    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
	int wait;
	FD_ZERO(&set);
	FD_SET(sock, &set);

	wait = minet_select(sock + 1, &set, 0, 0, 0);
	if(wait == -1)
	{
		minet_close(sock);
		minet_deinit();
		fprintf(stderr, "Select() returned with an error\n");
		exit(-1);
	} 

    /* first read loop -- read headers */
	int read_chk; 

	std::string head = "";
	std::string resp = "";
	std::string::size_type pos;

	memset(buf, 0, sizeof(buf));

	while((read_chk = minet_read(sock, buf, BUFSIZE-1)) > 0)
	{

		buf[read_chk] = '\0';
		resp += std::string(buf);
		if((pos = resp.find("\r\n\r\n", 0)) != std::string::npos)
		{
			head = resp.substr(0, pos);
			resp = resp.substr(pos+4);
			break;
		}		

	}
    
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200
	std::string str;
	int read_chk2;
	str = head.substr(head.find(" ")+1);
	str = str.substr(0, str.find(" "));

	if(str != "200")
	{ 
		fprintf(stderr, "%s\r\n\r\n", head.c_str());
		fprintf(stderr, "%s", resp.c_str());
		while((read_chk2 = minet_read(sock, buf, BUFSIZE-1)) > 0)
		{
			buf[read_chk2] = '\0';
			fprintf(stderr, buf);
		}
		fflush(stderr);
		minet_close(sock);
		minet_deinit();
		free(req);
		exit(-1);
	}
	else
	{
		fprintf(stdout, head.c_str());
		fprintf(stdout, "\r\n\r\n");
		fprintf(stdout, resp.c_str());

		while((read_chk2 = minet_read(sock, buf, BUFSIZE-1)) > 0)
		{
			buf[read_chk2] = '\0';
			fprintf(stdout, buf);
		}
	}

	free(req);
	minet_close(sock);
	minet_deinit();
	fflush(stdout);
	fflush(stderr);


    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}


