#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"


/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  int n_read = 0;
  while (n_read < len){
    int n = read(fd, &buf[n_read], len - n_read);
    if (n <= 0){
      return false;
    }
    else{
      n_read += n;
    }
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  int n_write = 0;
  while (n_write < len){
    int n = write(fd, &buf[n_write], len - n_write);
    if (n <= 0){
      return false;
    }
    else{
      n_write += n;
    }
  }
  return true;
}


/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint16_t len;
  uint8_t header[HEADER_LEN];

  if (!nread(fd, HEADER_LEN, header)){
    return false;
  }

  int offset = 0;
  memcpy(&len, header + offset, sizeof(len));
  offset += sizeof(len);
  memcpy(op, header + offset, sizeof(*op));
  offset += sizeof(*op);
  memcpy(ret, header + offset, sizeof(*ret));

  len = ntohs(len);
  *op = ntohl(*op);
  *ret = ntohs(*ret);

  if (len == (HEADER_LEN + JBOD_BLOCK_SIZE)){
    if (!nread(fd, 256, block)){
      return false;
    }
  }

  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t header[HEADER_LEN];
  uint16_t len;
  uint32_t cmd = op >> 26;

  if (cmd == JBOD_WRITE_BLOCK){
    len = (HEADER_LEN + JBOD_BLOCK_SIZE);
  }
  else {
    len = HEADER_LEN;
  }


  len = htons(len);
  op = htonl(op);

  int offset = 0;
  memcpy(header + offset, &len, sizeof(len));
  offset += sizeof(len);
  memcpy(header + offset, &op, sizeof(op));
  offset += sizeof(op);

  if (!nwrite(sd, HEADER_LEN, header)){
    return false;
  }
  if (cmd == JBOD_WRITE_BLOCK){
    if (!nwrite(sd, 256, block)){
      return false;
    }
  }

  return true;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
struct sockaddr_in caddr;
bool jbod_connect(const char *ip, uint16_t port) {

  // Setup address information
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  if (inet_aton(ip, &caddr.sin_addr) == 0){
    return false;
  }

  // Create socket
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1){
    return false;
  }

  // Connect to server
  if (connect(cli_sd, (const struct sockaddr*) &caddr, sizeof(caddr)) == -1){
    return false;
  }
  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t ret;
  send_packet(cli_sd, op, block);
  recv_packet(cli_sd, &op, &ret, block);
  return ret;
}
