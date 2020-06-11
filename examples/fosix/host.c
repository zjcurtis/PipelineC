// This is the main program to run on host for supporting fosix

#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../aws-fpga-dma/dma_msg_sw.c"
#include "protocol.h"

posix_h2c_t do_syscall_get_resp(posix_c2h_t req, dma_msg_t msg)
{
  posix_h2c_t resp;
  if(req.sys_open.req.valid)
  {
    // OPEN
    int fildes = open(req.sys_open.req.path, O_RDWR | O_CREAT); // TODO FLAGS
    resp.sys_open.resp.fildes = fildes;
    resp.sys_open.resp.valid = 1;
    if(fildes>255)
    {
      printf("File descriptor too large / err %d, %s ...TODO: fix.\n", fildes, req.sys_open.req.path);
      exit(-1);
    }
  }
  else if(req.sys_write.req.valid)
  {
    // WRITE
    resp.sys_write.resp.nbyte = write(req.sys_write.req.fildes, &(req.sys_write.req.buf[0]), req.sys_write.req.nbyte);
    resp.sys_write.resp.valid = 1;
    if(resp.sys_write.resp.nbyte != req.sys_write.req.nbyte)
    {
      printf("Bad write? fd %d, resp %d != req %d\n", req.sys_write.req.fildes, resp.sys_write.resp.nbyte, req.sys_write.req.nbyte);
      exit(-1);
    }
  }
  else if(req.sys_read.req.valid)
  {
    // READ
    resp.sys_read.resp.nbyte = read(req.sys_read.req.fildes, &(resp.sys_read.resp.buf[0]), req.sys_read.req.nbyte);
    resp.sys_read.resp.valid = 1;
    if(resp.sys_read.resp.nbyte != req.sys_read.req.nbyte)
    {
      printf("Bad read? fd %d, resp %d != req %d\n", req.sys_read.req.fildes, resp.sys_read.resp.nbyte, req.sys_read.req.nbyte);
      exit(-1);
    }
  }
  else if(req.sys_close.req.valid)
  {
    // CLOSE
    resp.sys_close.resp.err = close(req.sys_close.req.fildes);
    resp.sys_close.resp.valid = 1;
    if(resp.sys_close.resp.err)
    {
      printf("Close err? fd %d\n", req.sys_close.req.fildes);
      exit(-1);
    }
  }
  else
  {
    printf("TIMEOUT / UNKNOWN SYSTEM CALL REQUEST: %d\n", decode_syscall_id(msg));
    exit(-1);
  }
  printf("GOOD SYSTEM CALL REQUEST: %d\n", decode_syscall_id(msg));
  return resp;
}

// Init + control loop + close
//  TODO exit control loop correctly?
int main(int argc, char **argv) 
{
	// Init direct memory access to/from FPGA
  printf("Initializing direct memory access to/from FPGA...\n");
	init_dma();
  
	// Control loop
  printf("Beginning FOSIX Host control loop...\n");
  while(1)
  {
    // Read request dma msg
    //printf("Reading DMA msg...\n");
    dma_msg_t read_msg = dma_read();
    //printf("Read DMA msg...\n");
    
    // Convert to host request struct
    posix_c2h_t request = dma_to_request(read_msg);
    
    // Do the requested syscall and form response
    posix_h2c_t response = do_syscall_get_resp(request, read_msg);
    
    // Convert to dma message
    dma_msg_s write_msg = response_to_dma(response);
    
    // Write response dma msg 
    if(write_msg.valid)
    {
      //printf("Writing DMA msg...\n");
      dma_write(write_msg.data);
      //printf("Wrote DMA msg...\n");
    }
    else
    {
      printf("NO SYSTEM CALL RESPONSE\n");
      exit(-1);
    }
  }  

	// Close direct memory access to/from FPGA
	close_dma();    
}
