#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <algorithm>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc;
  int bind_chk;
  int listen_chk;
  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */
	if(toupper(*(argv[1])) == 'K') {
		minet_init(MINET_KERNEL);}
	else if(toupper(*(argv[1])) == 'U') {
		minet_init(MINET_USER);}
	else {
		fprintf(stderr, "First argument must be k or u\n");
		exit(-1);
	}

	sock = minet_socket(SOCK_STREAM);
	if(sock < 0)
	{
		fprintf(stderr, "There was an error when creating the socket\n");
		exit(-1);
	}
  /* set server address*/
	memset((char*) &sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(server_port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

  /* bind listening socket */
	bind_chk = minet_bind(sock, &sa);
	if(bind_chk < 0)
	{
		fprintf(stderr, "There was an error when binding the listening socket\n");
		exit(-1);
	}

  /* start listening */
	listen_chk = minet_listen(sock, 3);
	if(listen_chk < 0)
	{
		fprintf(stderr, "There was an error when trying to listen to the socket\n");
		exit(-1);
	}

  /* connection handling loop */
  while(1)
  {

	sock2 = minet_accept(sock, &sa2);
	if(sock2 < 0)
	{
		fprintf(stderr, "There was an error when accepting the socket\n");
		exit(-1);
	}
    /* handle connections */
    rc = handle_connection(sock2);
  }
}

int handle_connection(int sock2)
{
 // char filename[FILENAMESIZE+1];
  //int rc;
  int fd;
  int read_chk;
  struct stat filestat;
  char buf[BUFSIZE+1];
 // char *headers;
 // char *endheaders;
 // char *bptr;
 // int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
	memset(buf, 0, sizeof(buf));
	std::string head = "";
	std::string resp = "";
	std::string::size_type pos;

	while((read_chk = minet_read(sock2, buf, BUFSIZE-1)) > 0)
	{
		buf[read_chk] = '\0';
		resp += std::string(buf);
		if((pos = resp.find("\r\n\r\n", 0)) != std::string::npos)
		{
			head = resp.substr(0, pos);
			resp = resp.substr(pos+4);
			break;
		}
                //Adding this else if for telnet requests since they currently aren't being caught
                //i.e. sending "GET test.html HTTP/1.0" via telnet
                //shows up in the buffer as "GET test.html HTTP/1.0\r\n"
                else if((pos = resp.find("\r\n", 0)) != std::string::npos)
                {
                        head = resp.substr(0, pos);
                        if(strlen(buf) > pos + 2)
                        {
                                resp = resp.substr(pos+2);
                        }
                        else
                        {
                                resp = "";
                        }
                        break;
                }

	} 

	if(read_chk < 0)
	{
		fprintf(stderr, "There was an error when reading the request\n");
		minet_close(sock2);
		return -1;
	}	

  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
	std::string req_type;
	std::string file_name;

	if((pos = head.find(" ")) != std::string::npos)
	{
		req_type = head.substr(0, pos);
		head.erase(0, pos+1);
	}
	else
	{
		fprintf(stderr, "A valid request was not sent\n");
		minet_close(sock2);
		return -1;
	}

	std::transform(req_type.begin(), req_type.end(), req_type.begin(), toupper);

	if(req_type == "GET")
	{
		if((pos = head.find(" ")) != std::string::npos)
		{
			file_name = head.substr(0, pos);
		}
                //Again, adding this else if in for telnet connections
                //currently requests like "GET test2.html" throw the error since there is no second space to search for.
                else if(((pos = head.find(" ")) == std::string::npos) && head.length() > 0)
                {
                        file_name = head;
                }
		else
		{
			fprintf(stderr, "There is no filename to look up. Valid request not sent\n");
			minet_close(sock2);
			return -1;
		}
		
	}
	else
	{
		fprintf(stderr, "This is not a GET request\n");
		minet_close(sock2);
		return -1;
	}

    /* try opening the file */
	fd = open(file_name.c_str(), O_RDWR);
	if(fd == -1)
	{
		ok = false;
		fprintf(stderr, "File could not be opened\n");
	}

  /* send response */
  if (ok)
  {
    /* send headers */
	int head_chk;
	int stat_chk;
	int file_rd_chk;
	int file_wr_chk;

	stat_chk = stat(file_name.c_str(), &filestat);
	if(stat_chk < 0)
	{
		fprintf(stderr, "Could not get file stats\n");
		minet_close(sock2);
		return -1;
	} 

	sprintf(ok_response, ok_response_f, filestat.st_size);

	head_chk = minet_write(sock2, ok_response, strlen(ok_response));
	if(head_chk < 0)
	{
		fprintf(stderr, "Could not write header to socket 2\n");
		minet_close(sock2);
		return -1;
	}

    /* send file */
	while((file_rd_chk = minet_read(fd, buf, BUFSIZE-1)) > 0)
	{
		if(file_rd_chk < 0)
		{
			fprintf(stderr, "Could not read the file\n");
			minet_close(sock2);
			return -1;
		}

		file_wr_chk = minet_write(sock2, buf, file_rd_chk);
		if(file_wr_chk < 0)
		{
			fprintf(stderr, "Could not send the file to socket 2\n");
			minet_close(sock2);
			close(fd);
			return -1;
		}
	}

  }
  else // send error response
  {
	int error_wr_chk;

	error_wr_chk = minet_write(sock2, notok_response, strlen(notok_response));
	if(error_wr_chk < 0)
	{
		fprintf(stderr, "Could not write the error response to socket 2\n");
		minet_close(sock2);
		return -1;
	}
  }

  /* close socket and free space */

	minet_close(sock2);
	close(fd);

  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

