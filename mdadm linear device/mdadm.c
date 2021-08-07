#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

uint32_t encode_operation(jbod_cmd_t cmd, int disk_num, int block_num){
  // construct op
  // op = cmd << 26 | disk_num << 22 | block_num

  uint32_t op = 0x0, temp_cmd, temp_disk_num, temp_block_num;

  temp_block_num = block_num & 0xff;
  temp_disk_num = (disk_num & 0xff) << 22;
  temp_cmd = (cmd & 0xff) << 26;
  op = temp_block_num | temp_block_num | temp_cmd | temp_disk_num;
  
  return op;
}

  // block is NULL for mount, unmount, seek to disk, seek to block because none of these read or write data
  // read block and write block read or write data; second op should be a buffer

int mounted = 0;

int mdadm_mount(void) {
  uint32_t op = encode_operation(JBOD_MOUNT, 0, 0);
  int result = jbod_client_operation(op, NULL);
  if (result == 0){
    mounted = 1;
    return 1;
  }
  else{
    return -1;
  }
}

int mdadm_unmount(void) {
  uint32_t op = encode_operation(JBOD_UNMOUNT, 0, 0);
  int result = jbod_client_operation(op, NULL);
  if (result == 0){
    mounted = 0;
    return 1;
  }
  else{
    return -1;
  }
}

void translate_address(uint32_t linear_addr, int *disk_num, int *block_num, int *offset){
  *disk_num = linear_addr / JBOD_DISK_SIZE;
  *block_num = (linear_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  *offset = (linear_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

int seek(int disk_num, int block_num){
  int result1 = jbod_client_operation(encode_operation(JBOD_SEEK_TO_DISK, disk_num, 0), NULL);
  int result2 =jbod_client_operation(encode_operation(JBOD_SEEK_TO_BLOCK, 0, block_num), NULL);
  if (result1 || result2){
    return -1;
  }
  else{
    return -1;
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  if (mounted == 0){
    return -1;
  }
  if (len > 1024 || addr < 0 || addr + len > 1048576){
    return -1;
  }
  if (buf == NULL && len != 0){
    return -1;
  }


  int curr_addr = addr;
  int block_count = 0; // how many blocks read
  int n = len;
  int copy_of_len = len; // keeps track of how much have been read
  int disk_num, block_num, offset;

  while (curr_addr < addr + len){

    translate_address(curr_addr, &disk_num, &block_num, &offset);   // translate address
    seek(disk_num, block_num);  // seek to correct disk num and block num

    uint8_t tmp[JBOD_BLOCK_SIZE];
    int result = cache_lookup(disk_num, block_num, buf);
    if (result == -1)
    {
      jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp);
      cache_insert(disk_num, block_num, buf);
    }

    // first block
    if (block_count == 0){
      if (len + offset <= JBOD_BLOCK_SIZE){ // fits in one block
        memcpy(buf, tmp + offset, len);
        copy_of_len -= len;
        curr_addr += len;
      }
      else{
        n = JBOD_BLOCK_SIZE - offset; 
        memcpy(buf, tmp + offset, n);
        copy_of_len -= n;
        curr_addr += n;
      }
      block_count = 1; // not on first block anymore
    }

    // full blocks
    else if (copy_of_len >= 256){
      buf += n;
      memcpy(buf, tmp, JBOD_BLOCK_SIZE);
      copy_of_len -= 256;
      n = 256;
      curr_addr += 256;
    }

    // last block
    else{
      memcpy(buf + n, tmp, copy_of_len);
      curr_addr += copy_of_len;
    }
  }

  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if (mounted == 0){
    return -1;
  }
  if (len > 1024 || addr < 0 || addr + len > 1048576){
    return -1;
  }
  if (buf == NULL && len != 0){
    return -1;
  } 

  int curr_addr = addr;
  int block_count = 0; 
  int n = len;
  int copy_of_len = len;
  int disk_num, block_num, offset;
  int bytes_written = 0;

  while (curr_addr < addr + len){

    translate_address(curr_addr, &disk_num, &block_num, &offset);
    seek(disk_num, block_num);

    uint8_t tmp[JBOD_BLOCK_SIZE];
    

    // first block
    if (block_count == 0){
      if (len + offset <= JBOD_BLOCK_SIZE){ 
        // read block to tmp buf
        jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp);
        seek(disk_num, block_num);
        memcpy(tmp + offset, buf, len);
        jbod_client_operation(encode_operation(JBOD_WRITE_BLOCK, 0, 0), tmp);
        bytes_written += len;
        copy_of_len -= len;
        curr_addr += len;
      }
      else{
        jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp);
        seek(disk_num, block_num);  
        n = JBOD_BLOCK_SIZE - offset;             
        memcpy(tmp + offset, buf, n);
        bytes_written += n;
        jbod_client_operation(encode_operation(JBOD_WRITE_BLOCK, 0, 0), tmp);
        copy_of_len -= n;
        curr_addr += n;
      }
      block_count = 1; 
    }
    // full blocks
    else if (copy_of_len >= 256){
      memcpy(tmp, buf + bytes_written, JBOD_BLOCK_SIZE);
      jbod_client_operation(encode_operation(JBOD_WRITE_BLOCK, 0, 0), tmp);
      copy_of_len -= 256;
      n = 256;
      bytes_written += 256;
      curr_addr += 256;
    }
    // last block
    else{
      jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), tmp);
      seek(disk_num, block_num);  
      memcpy(tmp + offset, buf + bytes_written, copy_of_len);
      jbod_client_operation(encode_operation(JBOD_WRITE_BLOCK, 0, 0), tmp);
      curr_addr += copy_of_len;
    }
  }
  return len;
}