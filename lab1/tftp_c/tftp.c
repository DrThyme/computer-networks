/* A tftp client implementation.
   Author: Erik Nordstr�m <eriknord@it.uu.se>
*/

/* 
   Student implementation by:
   Tim Josefsson, 890411-1534
   Martin Kjellin, 791103-5918
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include "tftp.h"

extern int h_errno;

#define TFTP_TYPE_GET 0
#define TFTP_TYPE_PUT 1

/* Should cover most needs */
#define MSGBUF_SIZE (TFTP_DATA_HDR_LEN + BLOCK_SIZE)


/*
 * NOTE:
 * In tftp.h you will find definitions for headers and constants. Make
 * sure these are used throughout your code.
 */


/* A connection handle */
struct tftp_conn {
  int type; /* Are we putting or getting? */
  FILE *fp; /* The file we are reading or writing */
  int sock; /* Socket to communicate with server */
  int blocknr; /* The current block number */
  char *fname; /* The file name of the file we are putting or getting */
  char *mode; /* TFTP mode */
  struct sockaddr_in peer_addr; /* Remote peer address */
  socklen_t addrlen; /* The remote address length */
  char msgbuf[MSGBUF_SIZE]; /* Buffer for messages being sent or received */
};

/* Close the connection handle, i.e., delete our local state. */
void tftp_close(struct tftp_conn *tc)
{
  if (!tc)
    return;

  fclose(tc->fp);
  close(tc->sock);
  free(tc);
}

/* Connect to a remote TFTP server. */
struct tftp_conn *tftp_connect(int type, char *fname, char *mode,
			       const char *hostname)
{
  struct addrinfo hints;
  struct addrinfo * res = NULL;
  struct tftp_conn *tc;

  if (!fname || !mode || !hostname)
    return NULL;

  tc = malloc(sizeof(struct tftp_conn));

  if (!tc)
    return NULL;

  /* Create a socket.
   * Check return value. */

  /* ===ADDED=== */
  int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (socket_fd == -1) {
    free(tc);
    return NULL;
  } else {
    tc->sock = socket_fd;
  }
  /* ===END OF ADDED=== */

  if (type == TFTP_TYPE_PUT)
    tc->fp = fopen(fname, "rb");
  else if (type == TFTP_TYPE_GET)
    tc->fp = fopen(fname, "wb");
  else {
    fprintf(stderr, "Invalid TFTP mode, must be put or get.\n");
    return NULL;
  }

  if (tc->fp == NULL) {
    fprintf(stderr, "File I/O error.\n");
    close(tc->sock);
    free(tc);
    return NULL;
  }

  memset(&hints,0,sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  char port_str[5];
  sprintf(port_str, "%d", TFTP_PORT);

  /* get address from host name.
   * If error, gracefully clean up.*/

  /* ===ADDED=== */
  int status = getaddrinfo(hostname, port_str, &hints, &res);
  if (status == -1) {
    fclose(tc->fp);
    close(tc->sock);
    free(tc);
    return NULL;
  }
  /* ===END OF ADDED=== */

  /* Assign address to the connection handle.
   * You can assume that the first address in the hostent
   * struct is the correct one */

  memcpy(&tc->peer_addr, res->ai_addr, res->ai_addrlen);

  tc->addrlen = sizeof(struct sockaddr_in);

  tc->type = type;
  tc->mode = mode;
  tc->fname = fname;
  tc->blocknr = 0;

  memset(tc->msgbuf, 0, MSGBUF_SIZE);

  return tc;
}



/*
  Send a read request to the server
  1. Format message
  2. Send the request using the connection handle
  3. Return the number of bytes sent, or negative on error
*/
int tftp_send_rrq(struct tftp_conn *tc)
{
  /* ===ADDED/CHANGED=== */
  struct tftp_rrq *rrq; 
  rrq = malloc(TFTP_RRQ_LEN(tc->fname,tc->mode));

  rrq->opcode = htons(OPCODE_RRQ);
  int index = sprintf(rrq->req, "%s", tc->fname);
  index++;
  sprintf((rrq->req + index), "%s", tc->mode);

  int bytes_sent = sendto(tc->sock, rrq, TFTP_RRQ_LEN(tc->fname, tc->mode), 0,
			  (struct sockaddr *) &(tc->peer_addr), tc->addrlen);

  free(rrq);
  return bytes_sent;
  /* ===END OF ADDED/CHANGED=== */
}

/*

  Send a write request to the server.
  1. Format message.
  2. Send the request using the connection handle.
  3. Return the number of bytes sent, or negative on error.
*/
int tftp_send_wrq(struct tftp_conn *tc)
{
  /* ===ADDED/CHANGED=== */
  struct tftp_wrq *wrq;
  wrq = malloc(TFTP_WRQ_LEN(tc->fname,tc->mode));
  
  wrq->opcode = htons(OPCODE_WRQ);
  int index = snprintf(wrq->req, BLOCK_SIZE, "%s", tc->fname);

  index++;

  snprintf((wrq->req + index), (BLOCK_SIZE - index - 1), "%s", tc->mode);

  int bytes_sent = sendto(tc->sock, wrq, TFTP_WRQ_LEN(tc->fname, tc->mode), 0,
			  (struct sockaddr *) &(tc->peer_addr), tc->addrlen);

  free(wrq);

  return bytes_sent;
  /* ===END OF ADDED/CHANGED=== */
}

/*
  Acknowledge reception of a block.
  1. Format message.
  2. Send the acknowledgement using the connection handle.
  3. Return the number of bytes sent, or negative on error.
*/
int tftp_send_ack(struct tftp_conn *tc)
{

  /* ===ADDED/CHANGED=== */
  struct tftp_ack ack;
  
  ack.opcode = htons(OPCODE_ACK);
  ack.blocknr = htons(tc->blocknr);

  int bytes_sent = sendto(tc->sock, &ack, TFTP_ACK_HDR_LEN, 0,
			  (struct sockaddr *) &(tc->peer_addr), tc->addrlen);

  return bytes_sent;
  /* ===END OF ADDED/CHANGED=== */
}

/*
  Send a data block to the other side.
  1. Format message.
  2. Add data block to message according to length argument.
  3. Send the data block message using the connection handle.
  4. Return the number of bytes sent, or negative on error.

  TIP: You need to be able to resend data in case of a timeout. When
  resending, the old message should be sent again and therefore no
  new message should be created. This can, for example, be handled by
  passing a negative length indicating that the creation of a new
  message should be skipped.
*/
int tftp_send_data(struct tftp_conn *tc, int length)
{
  /* ===ADDED/CHANGED=== */
  struct tftp_data *tdata;
  
  int bytes_sent;

  if(length < 0 ){
    bytes_sent = sendto(tc->sock, tc->msgbuf, (-length), 0,
			(struct sockaddr *) &(tc->peer_addr), tc->addrlen);
  } else {
    tdata = malloc(TFTP_DATA_HDR_LEN+length);
    tdata->opcode = htons(OPCODE_DATA);
    tdata->blocknr = htons(tc->blocknr);

    fread(tdata->data, length, 1, tc->fp);

    memcpy(tc->msgbuf, tdata, TFTP_DATA_HDR_LEN+length);
    bytes_sent = sendto(tc->sock, tdata, TFTP_DATA_HDR_LEN+length, 0,
			(struct sockaddr *) &(tc->peer_addr), tc->addrlen);
    free(tdata);
  }
  return bytes_sent;
  /* ===END OF ADDED/CHANGED=== */
}

/*
  Transfer a file to or from the server.

*/
int tftp_transfer(struct tftp_conn *tc)
{
  int retval = 0;
  int len;
  int totlen = 0;
  struct timeval timeout;

  int loopend = 1;
  int final_pkt = 0;
  int first_rrq = 1;
  int first_wrq = 1;
  /* Sanity check */
  if (!tc)
    return -1;

  len = 0;

  /* ===ADDED (NOT ACCORDING TO INSTRUCTIONS)=== */
  long int file_size;
  fseek(tc->fp, 0L, SEEK_END);
  file_size = ftell(tc->fp);
  fseek(tc->fp, 0L, SEEK_SET);
  /* ===END OF ADDED=== */

  /* After the connection request we should start receiving data
   * immediately */

  /* Set a timeout for resending data. */

  timeout.tv_sec = TFTP_TIMEOUT;
  timeout.tv_usec = 0;

  /* Check if we are putting a file or getting a file and send
   * the corresponding request. */

  /* ===ADDED=== */
  if (tc->type == TFTP_TYPE_GET) {
    retval = tftp_send_rrq(tc);
  } else if (tc->type == TFTP_TYPE_PUT) {
    retval = tftp_send_wrq(tc);
  }

  if (retval == -1) {
    return retval;
  } else {
    totlen += retval;
  }
  /* ===END OF ADDED=== */

  /*
    Put or get the file, block by block, in a loop.
  */

  int all_done = 0;
  do {
    /* 1. Wait for something from the server (using
     * 'select'). If a timeout occurs, resend last block
     * or ack depending on whether we are in put or get
     * mode. */
    /* ===ADDED=== */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(tc->sock, &readfds);
    select(tc->sock + 1, &readfds, NULL, NULL, &timeout);
    if (FD_ISSET(tc->sock, &readfds)) {
      retval = recvfrom(tc->sock, tc->msgbuf, MSGBUF_SIZE, 0,
			(struct sockaddr *) &tc->peer_addr, &tc->addrlen);
    } else { // Timeout
      if (tc->type == TFTP_TYPE_PUT) {
	if (first_wrq == 1) {
	  retval = tftp_send_wrq(tc);
	} else {
	  retval = tftp_send_data(tc, -len);
	}
        continue;
      } else if (tc->type == TFTP_TYPE_GET) {
	if (first_rrq == 1) {
	  retval = tftp_send_rrq(tc);
	} else if(final_pkt == 1) {
	  break;
	} else {
	  retval = tftp_send_ack(tc);
	}
	continue;
      }
    }
    /* ===END OF ADDED=== */

    /* 2. Check the message type and take the necessary
     * action. */

    /* ===ADDED/CHANGED=== */
    u_int16_t received_opcode = htons(*((u_int16_t*)tc->msgbuf));

    u_int16_t received_blocknr;
    struct tftp_data *data;
    struct tftp_ack *ack_recv;
    struct tftp_err *err_recv;

    switch (received_opcode) {
    case OPCODE_DATA:
      /* Received data block, send ack */
      data = (struct tftp_data *) tc->msgbuf;
      totlen += retval;
      len = retval - TFTP_DATA_HDR_LEN;
      received_blocknr = ntohs(data->blocknr);
      if((tc->blocknr+1) == received_blocknr){
	fwrite(data->data, 1, len, tc->fp);
	if(len < BLOCK_SIZE) {
	  final_pkt = 1;
	}
	tc->blocknr++;
      }
      retval = tftp_send_ack(tc);
      first_rrq = 0;
      break;
    case OPCODE_ACK:
      /* Received ACK, send next block */
      ack_recv = (struct tftp_ack *) tc->msgbuf;
      received_blocknr = ntohs(ack_recv->blocknr);
      if(received_blocknr == tc->blocknr){
	if(all_done){
	  loopend = 0;
	} else {
	  tc->blocknr++;
	  long int file_pos = ftell(tc->fp);
	  if (file_size - file_pos < BLOCK_SIZE) {
	    retval = tftp_send_data(tc, file_size - file_pos);
	    all_done = 1;
	  } else {
	    retval = tftp_send_data(tc, BLOCK_SIZE);
	  }
	}
      } else {
	retval = tftp_send_data(tc, -len);
      }
      len = retval;
      first_wrq = 0;
      break;
    case OPCODE_ERR:
      err_recv = (struct tftp_err *) tc->msgbuf;
      printf("ERROR CODE %d: %s\n", ntohs(err_recv->errcode), err_recv->errmsg);

      goto out;
    /* ===END OF ADDED/CHANGED=== */
    default:
      fprintf(stderr, "\nUnknown message type\n");
      goto out;

    }

    /* ===ADDED=== */
      totlen += retval;
    /* ===END OF ADDED=== */

    /* ===CHANGED=== */
  } while (loopend /* 3. Loop until file is finished */);
  /* ===END OF CHANGED=== */
  printf("\nTotal data bytes sent/received: %d.\n", totlen);
 out:
  return retval;
}

int main (int argc, char **argv)
{

  char *fname = NULL;
  char *hostname = NULL;
  char *progname = argv[0];
  int retval = -1;
  int type = -1;
  struct tftp_conn *tc;

  /* Check whether the user wants to put or get a file. */
  while (argc > 0) {

    if (strcmp("-g", argv[0]) == 0) {
      fname = argv[1];
      hostname = argv[2];

      type = TFTP_TYPE_GET;
      break;
    } else if (strcmp("-p", argv[0]) == 0) {
      fname = argv[1];
      hostname = argv[2];

      type = TFTP_TYPE_PUT;
      break;
    }
    argc--;
    argv++;
  }

  /* Print usage message */
  if (!fname || !hostname) {
    fprintf(stderr, "Usage: %s [-g|-p] FILE HOST\n", progname);
    return -1;
  }

  /* Connect to the remote server */
  tc = tftp_connect(type, fname, MODE_OCTET, hostname);

  if (!tc) {
    fprintf(stderr, "Failed to connect.\n");
    return -1;
  }

  /* Transfer the file to or from the server */
  retval = tftp_transfer(tc);

  if (retval < 0) {
    fprintf(stderr, "File transfer failed.\n");
  }

  /* We are done. Cleanup our state. */
  tftp_close(tc);

  return retval;
}
