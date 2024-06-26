// Copy of single cycle CPU rewritten ~netlist/multi MAIN graph style
// Each MAIN function node in the graph instantiates N copies of barrel components
// For now has no flow control between stages, is always a rolling barrel.
//
// Also mixes in custom hardware accelerator piplines and such
// needs reorganization to reduce boilerplate and make more reusable
#pragma PART "xc7a100tcsg324-1"
#include "stream/stream.h"

// RISC-V components
#include "risc-v.h"
#include "mem_map.h"

// Shared with software memory map info
#include "gcc_test/mem_map.h"
DECL_STREAM_TYPE(kernel_in_t)
DECL_STREAM_TYPE(pixel_t)
DECL_STREAM_TYPE(uint32_t)

// LEDs for demo
#include "leds/leds_port.c"

// TODO REDO EVALUATING THREAD STAGE CONFIG NOW THAT MEM + EXE+ DECODE HAVE MORE STUFF
// Config threads
// Mhz               | ~40  ~55-60 ~100  ~158  ~164  (comb->  150  160  ...~fmax 400Mhz?
// Threads(~#stages) | 1    3      4     5     6              9    16      64?
// max ops/sec       |      ~1.6G  
// NEW 
// Threads(~#stages) | 3    4    5     ...EXE needs autopipeline ... 
// MHz               | ~61  ~86  ~141

// AXI Frame buffer RAM:
//#define AXI_RAM_MODE_BRAM // Only one frame buf used in this config, though two exist
#define AXI_RAM_MODE_DDR
#ifdef AXI_RAM_MODE_DDR
#define CPU_CLK_MHZ 83.33
#endif
#ifdef AXI_RAM_MODE_BRAM
#define CPU_CLK_MHZ 25.0
#endif
#define HOST_CLK_MHZ CPU_CLK_MHZ
#define NUM_USER_THREADS NUM_THREADS
#include "examples/shared_resource_bus/axi_frame_buffer/dual_frame_buffer.c"
#ifdef AXI_RAM_MODE_DDR
#define host_to_frame_buf host_clk_to_dev(axi_xil_mem)
#define frame_buf_to_host dev_to_host_clk(axi_xil_mem)
#endif
#ifdef AXI_RAM_MODE_BRAM
#define host_to_frame_buf host_clk_to_dev(axi_ram0_shared_bus)
#define frame_buf_to_host dev_to_host_clk(axi_ram0_shared_bus)
#endif
DECL_AXI_SHARED_BUS_WRITE_START(RISCV_RAM_NUM_BURST_WORDS)
#define axi_shared_bus_burst_write_start axi_shared_bus_write_start(RISCV_RAM_NUM_BURST_WORDS)
DECL_AXI_SHARED_BUS_READ_FINISH(RISCV_RAM_NUM_BURST_WORDS)
#define axi_shared_bus_burst_read_finish_t axi_shared_bus_read_finish_t(RISCV_RAM_NUM_BURST_WORDS)
#define axi_shared_bus_burst_read_finish axi_shared_bus_read_finish(RISCV_RAM_NUM_BURST_WORDS)


// Declare a shared kernel pipeline
#define SHARED_RESOURCE_BUS_PIPELINE_NAME         kernel
#define SHARED_RESOURCE_BUS_PIPELINE_OUT_TYPE     pixel_t
#define SHARED_RESOURCE_BUS_PIPELINE_FUNC         kernel_func
#define SHARED_RESOURCE_BUS_PIPELINE_IN_TYPE      kernel_in_t
#define SHARED_RESOURCE_BUS_PIPELINE_HOST_THREADS NUM_THREADS
#define SHARED_RESOURCE_BUS_PIPELINE_HOST_CLK_MHZ CPU_CLK_MHZ
#define SHARED_RESOURCE_BUS_PIPELINE_DEV_CLK_MHZ  CPU_CLK_MHZ
#include "shared_resource_bus_pipeline.h"
#define host_to_kernel host_clk_to_dev(kernel)
#define kernel_to_host dev_to_host_clk(kernel)

// Hardware to sync threads and also toggle frame buffer select
uint1_t thread_is_done_signal[N_BARRELS][N_THREADS_PER_BARREL];
uint1_t next_thread_start_signal[N_BARRELS][N_THREADS_PER_BARREL];
#pragma MAIN thread_sync_module
void thread_sync_module(){
  // Wait for all threads to signal done with expected value
  // when all done toggle output to start next frame
  static uint1_t expected_signal_value = 1;
  static uint1_t threads_are_done_r; // Register to reduce data path length
  uint1_t threads_done = 1;
  uint32_t bid;
  for(bid = 0; bid < N_BARRELS; bid+=1)
  {
    uint8_t tid;
    for(tid = 0; tid < N_THREADS_PER_BARREL; tid+=1)
    {
      threads_done &= (thread_is_done_signal[bid][tid]==expected_signal_value);
      next_thread_start_signal[bid][tid] = expected_signal_value;
    }
  }
  if(threads_are_done_r){
    expected_signal_value = ~expected_signal_value;
    #ifndef AXI_RAM_MODE_BRAM
    frame_buffer_read_port_sel = ~frame_buffer_read_port_sel;
    #endif
    threads_done = 0;
  }
  threads_are_done_r = threads_done;
}


// Produce a configurable stream of x,y,frame_count values for shader kernel input
typedef struct xy_frame_count_source_t
{
  int32_t x;
  int32_t y;
  uint32_t frame_count;
  uint1_t valid;
  uint1_t done;
}xy_frame_count_source_t;
xy_frame_count_source_t
xy_frame_count_source(
  int32_t start_x,
  int32_t start_y,
  uint32_t frame_count,
  uint32_t num_pixels,
  uint1_t inputs_valid,
  uint1_t ready
){
  xy_frame_count_source_t o;
  static uint1_t done;
  static int32_t x;
  static uint1_t valid;
  static uint32_t count;
  o.done = done;
  o.x = x;
  o.y = start_y;
  o.frame_count = frame_count;
  o.valid = valid;
  if(~done & inputs_valid){
    if(o.valid & ready){
      if(count==(num_pixels-1)){
        done = 1;
        valid = 0;
      }else{
        count += 1;
        x += 1;
      }
    }
    if(count==0){
      x = start_x;
      valid = 1;
    }
  }
  if(~inputs_valid){
    count = 0;
    done = 0;
    valid = 0;
  }
  return o;
}


// Module to join streams of pixels and x,y,frame count
// TOOD generic version of this?
typedef struct kernel_input_join_t{
  kernel_in_t kernel_in;
  uint1_t kernel_in_valid;
  uint1_t pixel_stream_ready;
  uint1_t xy_frame_stream_ready;
}kernel_input_join_t;
kernel_input_join_t kernel_input_join(
  uint1_t kernel_in_ready,
  stream(pixel_t) pixel_stream,
  int32_t x, int32_t y, uint32_t frame_count,
  uint1_t xy_frame_valid
){
  // GAH need skid buffer regs for now todo fix valid+ready dependence in shared bus stuff
  static stream(pixel_t) pixel_stream_r;
  static int32_t x_r;
  static int32_t y_r;
  static uint32_t frame_count_r;
  static uint1_t xy_frame_valid_r;
  // Only valid data if both inputs valid
  kernel_input_join_t o;
  o.kernel_in_valid = pixel_stream_r.valid & xy_frame_valid_r;
  o.kernel_in.x = x_r;
  o.kernel_in.y = y_r;
  o.kernel_in.frame_count = frame_count_r;
  o.kernel_in.pixel_in = pixel_stream_r.data;
  // Clear buffer if kernel accepts input
  if(o.kernel_in_valid & kernel_in_ready){
    pixel_stream_r.valid = 0;
    xy_frame_valid_r = 0;
  }
  // Only ready for inputs when room in buffer
  if(~pixel_stream_r.valid){
    o.pixel_stream_ready = 1;
    pixel_stream_r = pixel_stream;
  }
  if(~xy_frame_valid_r){
    o.xy_frame_stream_ready = 1;
    x_r = x;
    y_r = y;
    frame_count_r = frame_count;
    xy_frame_valid_r = xy_frame_valid;
  }
  return o;
}


// Declare memory map information
// TODO make+use macros for boilerplate handshake logic below...
// Define MMIO inputs and outputs
typedef struct my_mmio_in_t{
  uint32_t thread_id;
  uint32_t frame_signal;
}my_mmio_in_t;
typedef struct my_mmio_out_t{
  uint32_t return_value;
  uint1_t halt; // return value valid
  uint1_t led;
  uint32_t frame_signal;
}my_mmio_out_t;
// Define the hardware memory for those IO
RISCV_DECL_MEM_MAP_MOD_OUT_T(my_mmio_out_t)
riscv_mem_map_mod_out_t(my_mmio_out_t) my_mem_map_module(
  RISCV_MEM_MAP_MOD_INPUTS(my_mmio_in_t)
){
  // Outputs
  static riscv_mem_map_mod_out_t(my_mmio_out_t) o;
  o.addr_is_mapped = 0; // since o is static regs

  // Handshake regs
  //  Frame buffer RAM
  static riscv_ram_write_req_t ram_write_req_reg;
  riscv_ram_write_req_t ram_write_req = ram_write_req_reg;
  static riscv_ram_write_resp_t ram_write_resp_reg;
  riscv_ram_write_resp_t ram_write_resp = ram_write_resp_reg;
  static riscv_ram_read_req_t ram_read_req_reg;
  riscv_ram_read_req_t ram_read_req = ram_read_req_reg;
  static riscv_ram_read_resp_t ram_read_resp_reg;
  riscv_ram_read_resp_t ram_read_resp = ram_read_resp_reg;
  //  Kernel func
  static kernel_in_t kernel_data_in_reg;
  kernel_in_t kernel_data_in = kernel_data_in_reg;
  static uint32_t kernel_data_in_valid_reg;
  uint32_t kernel_data_in_valid = kernel_data_in_valid_reg;
  static pixel_t kernel_data_out_reg;
  pixel_t kernel_data_out = kernel_data_out_reg;
  static uint32_t kernel_data_out_valid_reg;
  uint32_t kernel_data_out_valid = kernel_data_out_valid_reg;

  // Dataflow control+status
  static dataflow_mm_t data_flow_mm_reg;
  dataflow_mm_t data_flow_mm = data_flow_mm_reg;  

  // Memory muxing/select logic
  // Uses helper comparing word address and driving a variable
  o.outputs.return_value = inputs.thread_id; // Override typical reg read behavior
  WORD_MM_ENTRY(o, THREAD_ID_RETURN_OUTPUT_ADDR, o.outputs.return_value)
  o.outputs.halt = wr_byte_ens[0] & (addr==THREAD_ID_RETURN_OUTPUT_ADDR);
  WORD_MM_ENTRY(o, LED_ADDR, o.outputs.led)

  // Mem map the handshake regs
  //  Frame buffer RAM
  STRUCT_MM_ENTRY(o, RAM_WR_REQ_ADDR, riscv_ram_write_req_t, ram_write_req_reg)
  STRUCT_MM_ENTRY(o, RAM_WR_RESP_ADDR, riscv_ram_write_resp_t, ram_write_resp_reg)
  STRUCT_MM_ENTRY(o, RAM_RD_REQ_ADDR, riscv_ram_read_req_t, ram_read_req_reg)
  STRUCT_MM_ENTRY(o, RAM_RD_RESP_ADDR, riscv_ram_read_resp_t, ram_read_resp_reg)
  //  Kernel func
  STRUCT_MM_ENTRY_NEW(KERNEL_DATA_IN_ADDR, kernel_in_t, kernel_data_in_reg, kernel_data_in_reg, addr, o.addr_is_mapped, o.rd_data)
  WORD_MM_ENTRY_NEW(KERNEL_VALID_IN_ADDR, kernel_data_in_valid_reg, kernel_data_in_valid_reg, addr, o.addr_is_mapped, o.rd_data)
  STRUCT_MM_ENTRY_NEW(KERNEL_DATA_OUT_ADDR, pixel_t, kernel_data_out_reg, kernel_data_out_reg, addr, o.addr_is_mapped, o.rd_data)
  WORD_MM_ENTRY_NEW(KERNEL_VALID_OUT_ADDR, kernel_data_out_valid_reg, kernel_data_out_valid_reg, addr, o.addr_is_mapped, o.rd_data)
  
  // Data flow control and status
  STRUCT_MM_ENTRY_NEW(DATAFLOW_MM_ADDR, dataflow_mm_t, data_flow_mm_reg, data_flow_mm_reg, addr, o.addr_is_mapped, o.rd_data)

  // Mem map frame sync signal
  if(addr==FRAME_SIGNAL_ADDR){
    o.addr_is_mapped = 1;
    o.rd_data = inputs.frame_signal;
    if(wr_byte_ens[0]){
      o.outputs.frame_signal = wr_data;
    }
  }

  // Handshake logic using signals from regs
  // Default null
  host_to_frame_buf = axi_shared_bus_t_HOST_TO_DEV_NULL;
  host_to_kernel = kernel_bus_t_HOST_TO_DEV_NULL;
  #ifdef AXI_RAM_MODE_BRAM
  // Extra unused second frame buf
  axi_ram1_shared_bus_host_to_dev_wire_on_host_clk = axi_shared_bus_t_HOST_TO_DEV_NULL; // UNUSED
  axi_shared_bus_t_dev_to_host_t dummy_read = axi_ram1_shared_bus_dev_to_host_wire_on_host_clk;// ?
  #endif

  

  // Mux between CPU thread bus and data flow bus
  axi_shared_bus_t_host_to_dev_t thread_to_frame_buf;
  axi_shared_bus_t_host_to_dev_t dataflow_to_frame_buf;
  axi_shared_bus_t_dev_to_host_t frame_buf_to_thread;
  axi_shared_bus_t_dev_to_host_t frame_buf_to_dataflow;
  if(data_flow_mm.all_ports_dataflow_host){
    frame_buf_to_dataflow = frame_buf_to_host;
  }else{
    frame_buf_to_thread = frame_buf_to_host;
  }
  kernel_bus_t_host_to_dev_t thread_to_kernel;
  kernel_bus_t_host_to_dev_t dataflow_to_kernel;
  kernel_bus_t_dev_to_host_t kernel_to_thread;
  kernel_bus_t_dev_to_host_t kernel_to_dataflow;
  if(data_flow_mm.all_ports_dataflow_host){
    kernel_to_dataflow = kernel_to_host;
  }else{
    kernel_to_thread = kernel_to_host;
  }


  // Write start
  // SW sets req valid, hardware clears when accepted
  if(ram_write_req.valid){
    uint32_t waddr;
    #ifdef AXI_RAM_MODE_BRAM
    waddr = ram_write_req.addr;
    #else
    waddr = dual_ram_to_addr(~frame_buffer_read_port_sel, ram_write_req.addr);
    #endif
    axi_shared_bus_write_start_t write_start = axi_shared_bus_burst_write_start(
      1,
      waddr,
      ram_write_req.data,
      ram_write_req.num_words,
      frame_buf_to_thread.write
    );
    thread_to_frame_buf.write = write_start.to_dev;
    if(write_start.done){
      ram_write_req_reg.valid = 0;
    }
  }
  // Read start
  // SW sets req valid, hardware clears when accepted
  if(ram_read_req.valid){
    uint32_t raddr;
    #ifdef AXI_RAM_MODE_BRAM
    raddr = ram_read_req.addr;
    #else
    raddr = dual_ram_to_addr(frame_buffer_read_port_sel, ram_read_req.addr);
    #endif
    axi_shared_bus_sized_read_start_t read_start = axi_shared_bus_sized_read_start(
      raddr,
      ram_read_req.num_words,
      1,
      frame_buf_to_thread.read.req_ready
    );
    thread_to_frame_buf.read.req = read_start.bus_req;
    if(read_start.done){
      ram_read_req_reg.valid = 0;
    }
  }
  // Write Finish
  // HW sets resp valid, hardware auto clears when read by software
  // (since not using AXI write resp data, only valid flag)
  //  TODO is it better to just have write behave like read 
  //  and make software clear the resp valid reg?
  if(ram_write_resp.valid){
    if(rd_byte_ens[0] & ADDR_IS_VALID_FLAG(addr, RAM_WR_RESP_ADDR, riscv_ram_write_resp_t)){
      ram_write_resp_reg.valid = 0;
      ram_write_resp_reg.num_words = 0;
    }
  }else{
    if(ram_write_resp.num_words > 0){
      axi_shared_bus_sized_write_finish_t write_finish = axi_shared_bus_sized_write_finish(
        ram_write_resp_reg.num_words,
        1,
        frame_buf_to_thread.write
      );
      thread_to_frame_buf.write.resp_ready = write_finish.bus_resp_ready;
      if(write_finish.done){
        ram_write_resp_reg.valid = 1;
      }
    }
  }
  // Read finish
  // HW sets resp valid, software clears when accepted
  if(!ram_read_resp.valid & (ram_read_resp.num_words > 0)){
    axi_shared_bus_burst_read_finish_t read_finish = axi_shared_bus_burst_read_finish(
      ram_read_resp.num_words,
      1,
      frame_buf_to_thread.read.data
    );
    thread_to_frame_buf.read.data_ready = read_finish.bus_data_ready;
    if(read_finish.done){
      ram_read_resp_reg.data = read_finish.words;
      ram_read_resp_reg.valid = 1;
    }
  }


  // Kernel start (read req)
  // SW sets req valid, hardware clears when accepted
  kernel_bus_t_read_start_logic_outputs_t kernel_start = kernel_bus_t_read_start_logic(
    kernel_data_in, kernel_data_in_valid, 
    kernel_to_thread.read.req_ready
  );
  thread_to_kernel.read.req = kernel_start.req;
  if(kernel_start.done){
    kernel_data_in_valid_reg = 0;
  }

  // Kernel finish (read resp)
  // HW sets resp valid, software clears when accepted
  kernel_bus_t_read_finish_logic_outputs_t kernel_finish = kernel_bus_t_read_finish_logic(
    ~kernel_data_out_valid,
    kernel_to_thread.read.data
  );
  thread_to_kernel.read.data_ready = kernel_finish.data_ready;
  if(kernel_finish.done){
    kernel_data_out_reg = kernel_finish.data;
    kernel_data_out_valid_reg = 1;
  }


  // Dataflow AXI RAM read/write stream
  uint1_t dataflow_frame_buf_rd_stream_ready;
  #pragma FEEDBACK dataflow_frame_buf_rd_stream_ready
  stream(uint32_t) dataflow_frame_buf_wr_stream;
  #pragma FEEDBACK dataflow_frame_buf_wr_stream
  axi_shared_bus_to_stream_source_sink_t axi_source_sink = axi_shared_bus_to_stream_source_sink(
    frame_buf_to_dataflow,
    #ifdef AXI_RAM_MODE_DDR
    dual_ram_to_addr(~frame_buffer_read_port_sel, data_flow_mm.addr),
    #else
    data_flow_mm.addr,
    #endif
    data_flow_mm.num_pixels,
    data_flow_mm.valid,
    dataflow_frame_buf_wr_stream.data,
    dataflow_frame_buf_wr_stream.valid,
    #ifdef AXI_RAM_MODE_DDR
    dual_ram_to_addr(frame_buffer_read_port_sel, data_flow_mm.addr),
    #else
    data_flow_mm.addr,
    #endif
    data_flow_mm.num_pixels,
    data_flow_mm.valid,
    dataflow_frame_buf_rd_stream_ready
  );
  dataflow_to_frame_buf = axi_source_sink.to_dev;
  
  // Dataflow x,y,frame count stream source
  uint1_t xy_frame_count_stream_ready;
  #pragma FEEDBACK xy_frame_count_stream_ready
  xy_frame_count_source_t xy_frame_count = xy_frame_count_source(
    data_flow_mm.x,
    data_flow_mm.y,
    data_flow_mm.frame_count,
    data_flow_mm.num_pixels,
    data_flow_mm.valid,
    xy_frame_count_stream_ready
  );
  int32_t x_stream = xy_frame_count.x;
  int32_t y_stream = xy_frame_count.y;
  int32_t frame_count_stream = xy_frame_count.frame_count;
  uint1_t xy_frame_count_stream_valid = xy_frame_count.valid;

  // Dataflow kernel pipeline source and sink streams
  stream(kernel_in_t) kernel_in_stream;
  #pragma FEEDBACK kernel_in_stream
  uint1_t kernel_out_stream_ready;
  #pragma FEEDBACK kernel_out_stream_ready
  kernel_to_streams_t kernel_streams = kernel_to_streams(
    kernel_to_dataflow.read,
    kernel_in_stream,
    data_flow_mm.num_pixels,
    data_flow_mm.valid,
    kernel_out_stream_ready,
    data_flow_mm.num_pixels,
    data_flow_mm.valid
  );
  dataflow_to_kernel.read = kernel_streams.to_dev;
  
  // CUSTOM DATAFLOW AREA \/ |///////////////////////////////////////////////////////
  // Convert u32 from ram rd to pixel
  uint8_t rd_bytes[4];
  UINT32_T_TO_BYTES(rd_bytes, axi_source_sink.rd_stream_data)
  stream(pixel_t) rd_pixel_stream;
  rd_pixel_stream.data = bytes_to_pixel_t(rd_bytes);
  rd_pixel_stream.valid = axi_source_sink.rd_stream_valid;

  // Join the stream of read pixels and x,y,frame count to form kernel inputs
  kernel_input_join_t kernel_in_join = kernel_input_join(
    kernel_streams.in_stream_ready,
    rd_pixel_stream,
    x_stream, y_stream, frame_count_stream,
    xy_frame_count_stream_valid
  );
  kernel_in_stream.data = kernel_in_join.kernel_in;
  kernel_in_stream.valid = kernel_in_join.kernel_in_valid;
  dataflow_frame_buf_rd_stream_ready = kernel_in_join.pixel_stream_ready;
  xy_frame_count_stream_ready = kernel_in_join.xy_frame_stream_ready;

  // Convert kernel out pixel to u32 for ram wr
  uint8_t wr_bytes[4] = pixel_t_to_bytes(kernel_streams.out_stream.data);
  dataflow_frame_buf_wr_stream.data = bytes_to_uint32_t(wr_bytes);
  dataflow_frame_buf_wr_stream.valid = kernel_streams.out_stream.valid;
  kernel_out_stream_ready = axi_source_sink.wr_stream_ready;

  // Done when the writes to frame buf are done
  data_flow_mm_reg.done = axi_source_sink.wr_done;
  // CUSTOM DATAFLOW AREA /\ |///////////////////////////////////////////////////////

  // Mux between CPU thread bus and data flow bus
  if(data_flow_mm.all_ports_dataflow_host){
    host_to_frame_buf = dataflow_to_frame_buf;
  }else{
    host_to_frame_buf = thread_to_frame_buf;
  }
  if(data_flow_mm.all_ports_dataflow_host){
    host_to_kernel = dataflow_to_kernel;
  }else{
    host_to_kernel = thread_to_kernel;
  }
  
  return o;
}


// Per thread IO
uint1_t mem_out_of_range[N_BARRELS][N_THREADS_PER_BARREL]; // Exception, stop sim
uint1_t unknown_op[N_BARRELS][N_THREADS_PER_BARREL]; // Exception, stop sim
my_mmio_in_t  mem_map_inputs[N_BARRELS][N_THREADS_PER_BARREL];
my_mmio_out_t mem_map_outputs[N_BARRELS][N_THREADS_PER_BARREL];
// LEDs for demo
#include "leds/leds_port.c"
MAIN_MHZ(mm_io_connections, CPU_CLK_MHZ)
#pragma FUNC_WIRES mm_io_connections
void mm_io_connections()
{
  // Output LEDs for hardware debug
  // led0 is core0 led for some sign of life
  leds = 0;
  leds |= ((uint4_t)mem_map_outputs[0][0].led << 0);
  // Error flag from all cores
  static uint1_t mem_out_of_range_reg[N_BARRELS][N_THREADS_PER_BARREL]; // Exception, stop sim
  static uint1_t unknown_op_reg[N_BARRELS][N_THREADS_PER_BARREL]; // Exception, stop sim
  static uint1_t halt_reg[N_BARRELS][N_THREADS_PER_BARREL];
  static uint1_t mem_out_of_range_led;
  static uint1_t unknown_op_led;
  static uint1_t halt_led;
  leds |= ((uint4_t)mem_out_of_range_led << 3);
  leds |= ((uint4_t)unknown_op_led << 2);
  leds |= ((uint4_t)halt_led << 1);
  uint32_t bid;
  for(bid = 0; bid < N_BARRELS; bid+=1)
  {
    uint8_t tid;
    for(tid = 0; tid < N_THREADS_PER_BARREL; tid+=1)
    {
      mem_map_inputs[bid][tid].thread_id = (bid*N_THREADS_PER_BARREL) + tid;
      mem_map_inputs[bid][tid].frame_signal = next_thread_start_signal[bid][tid];
      thread_is_done_signal[bid][tid] = mem_map_outputs[bid][tid].frame_signal;
      unknown_op_led |= unknown_op_reg[bid][tid]; // Sticky
      mem_out_of_range_led |= mem_out_of_range_reg[bid][tid]; // Sticky
      halt_led |= halt_reg[bid][tid]; // Sticky
      halt_reg[bid][tid] = mem_map_outputs[bid][tid].halt;
    }
  }
  mem_out_of_range_reg = mem_out_of_range;
  unknown_op_reg = unknown_op;
}


// Declare a combined instruction and data memory
// also includes memory mapped IO
// Include test gcc compiled program
#include "gcc_test/mem_init.h" // MEM_INIT,MEM_INIT_SIZE
#define riscv_name              my_riscv
#define RISCV_MEM_INIT          MEM_INIT // from gcc_test
#define RISCV_MEM_SIZE_BYTES    MEM_INIT_SIZE // from gcc_test
#define riscv_mem_map           my_mem_map_module
#define riscv_mem_map_inputs_t  my_mmio_in_t
#define riscv_mem_map_outputs_t my_mmio_out_t
#define riscv_mmio_mod_out_t riscv_mem_map_mod_out_t(riscv_mem_map_outputs_t)
#define RISCV_MEM_1_CYCLE
#include "mem_decl.h" // declare my_riscv_mem_out my_riscv_mem() func


// Interconnect wires between stages
typedef struct thread_context_t{
  uint8_t thread_id; // TODO resolve as program_id, program_valid; Since different from thread id
  uint1_t thread_valid;
  uint32_t pc;
  uint32_t next_pc;
  uint32_t instruction;
  decoded_t decoded;
  reg_file_out_t reg_file_rd_datas;
  execute_t exe;
  uint32_t mem_rd_data;
}thread_context_t;
thread_context_t pc_inputs[N_BARRELS];
thread_context_t pc_outputs[N_BARRELS];
thread_context_t imem_inputs[N_BARRELS];
thread_context_t imem_outputs[N_BARRELS];
thread_context_t decode_inputs[N_BARRELS];
thread_context_t decode_outputs[N_BARRELS];
thread_context_t reg_rd_inputs[N_BARRELS];
thread_context_t reg_rd_outputs[N_BARRELS];
thread_context_t exe_inputs[N_BARRELS];
thread_context_t exe_outputs[N_BARRELS];
thread_context_t dmem_inputs[N_BARRELS];
thread_context_t dmem_outputs[N_BARRELS];
thread_context_t reg_wr_inputs[N_BARRELS];
thread_context_t reg_wr_outputs[N_BARRELS];
thread_context_t branch_inputs[N_BARRELS];
thread_context_t branch_outputs[N_BARRELS];
#pragma MAIN inter_stage_connections
#pragma FUNC_WIRES inter_stage_connections
void inter_stage_connections(){
  // Each inter stage connection is configurable
  // PC stage is 1 cycle delay +
  // All 8 interconnect delay regs
  // = baseline 9 threads from handpipelining alone
  #ifdef pc_to_imem_REG
  static thread_context_t pc_to_imem[N_BARRELS];
  imem_inputs = pc_to_imem;
  pc_to_imem = pc_outputs;
  #else
  imem_inputs = pc_outputs;
  #endif
  #ifdef imem_to_decode_REG
  static thread_context_t imem_to_decode[N_BARRELS];
  decode_inputs = imem_to_decode;
  imem_to_decode = imem_outputs;
  #else
  decode_inputs = imem_outputs;
  #endif
  #ifdef decode_to_reg_rd_REG
  static thread_context_t decode_to_reg_rd[N_BARRELS];
  reg_rd_inputs = decode_to_reg_rd;
  decode_to_reg_rd = decode_outputs;
  #else
  reg_rd_inputs = decode_outputs;
  #endif
  #if N_THREADS_PER_BARREL >= 5
  #define reg_rd_to_exe_REG
  #endif
  #ifdef reg_rd_to_exe_REG
  static thread_context_t reg_rd_to_exe[N_BARRELS];
  exe_inputs = reg_rd_to_exe;
  reg_rd_to_exe = reg_rd_outputs;
  #else
  exe_inputs = reg_rd_outputs;
  #endif
  #if N_THREADS_PER_BARREL >= 4
  #define exe_to_dmem_REG
  #endif
  #ifdef exe_to_dmem_REG
  static thread_context_t exe_to_dmem[N_BARRELS];
  dmem_inputs = exe_to_dmem;
  exe_to_dmem = exe_outputs;
  #else
  dmem_inputs = exe_outputs;
  #endif
  #ifdef dmem_to_reg_wr_REG
  static thread_context_t dmem_to_reg_wr[N_BARRELS];
  reg_wr_inputs = dmem_to_reg_wr;
  dmem_to_reg_wr = dmem_outputs;
  #else
   reg_wr_inputs = dmem_outputs;
  #endif
  #ifdef reg_wr_to_branch_REG
  static thread_context_t reg_wr_to_branch[N_BARRELS];
  branch_inputs = reg_wr_to_branch;
  reg_wr_to_branch = reg_wr_outputs;
  #else
  branch_inputs = reg_wr_outputs;
  #endif
  #ifdef branch_to_pc_REG
  static thread_context_t branch_to_pc[N_BARRELS];
  pc_inputs = branch_to_pc;
  branch_to_pc = branch_outputs;
  #else
  pc_inputs = branch_outputs;
  #endif
}


// Temp control fsm trying to start up to max threads
uint1_t barrel_start_ready[N_BARRELS];
uint32_t barrel_start_pc[N_BARRELS];
uint8_t barrel_start_thread_id[N_BARRELS];
uint1_t barrel_start_valid[N_BARRELS];
#pragma MAIN thread_starter_fsm
void thread_starter_fsm(){
  static uint8_t running_threads[N_BARRELS];
  uint32_t i;
  for (i = 0; i < N_BARRELS; i+=1)
  {
    barrel_start_valid[i] = running_threads[i] < N_THREADS_PER_BARREL;
    barrel_start_thread_id[i] = running_threads[i];
    barrel_start_pc[i] = 0;
    if(barrel_start_valid[i] & barrel_start_ready[i]){
      printf("Barrel %d Thread %d: Started!\n", i, barrel_start_thread_id[i]);
      running_threads[i] += 1;
    }
  }
}


// Current PC reg + thread ID + valid bit
typedef struct pc_stage_out_t{
  thread_context_t outputs;
  uint1_t start_ready;
}pc_stage_out_t;
pc_stage_out_t pc_stage(
  uint8_t bid, // Barrel ID for sim printing
  thread_context_t inputs,
  uint32_t start_thread_id,
  uint32_t start_pc,
  uint1_t start_valid
){
  // These regs give this pc stage 1 cycle of delay as baseline (not comb pass through)
  static uint32_t pc_reg;
  static uint8_t thread_id_reg;
  static uint1_t thread_valid_reg;
  pc_stage_out_t o;
  o.outputs.pc = pc_reg;
  o.outputs.thread_id = thread_id_reg;
  o.outputs.thread_valid = thread_valid_reg;
  if(o.outputs.thread_valid)
  {
    printf("Barrel %d Thread %d: PC = 0x%X\n", bid, o.outputs.thread_id, o.outputs.pc);
  }
  pc_reg = 0;
  thread_id_reg = 0;
  thread_valid_reg = 0;
  o.start_ready = 0;
  if(inputs.thread_valid){
    pc_reg = inputs.next_pc;
    thread_id_reg = inputs.thread_id;
    thread_valid_reg = 1;
  }else{
    // No next thread input to stage, accept stage
    o.start_ready = 1;
    if(start_valid){
      pc_reg = start_pc;
      thread_id_reg = start_thread_id;
      thread_valid_reg = 1;
    }
  }
  return o;
}
#pragma MAIN pc_thread_start_stage
#pragma FUNC_LATENCY pc_thread_start_stage 1
void pc_thread_start_stage(){
  uint32_t i;
  for (i = 0; i < N_BARRELS; i+=1)
  {
    pc_stage_out_t pc_stage_out = pc_stage(
      i,
      pc_inputs[i],
      barrel_start_thread_id[i],
      barrel_start_pc[i],
      barrel_start_valid[i]);
    pc_outputs[i] = pc_stage_out.outputs;
    barrel_start_ready[i] = pc_stage_out.start_ready;
  }
}


// Main memory  instance
// IMEM, 1 rd port
// DMEM, 1 RW port
typedef struct multi_thread_mem_stage_out_t
{
  thread_context_t imem_out;
  thread_context_t dmem_out;
  riscv_mem_map_outputs_t mem_map_out[N_THREADS_PER_BARREL];
  uint1_t mem_out_of_range[N_THREADS_PER_BARREL];
}multi_thread_mem_stage_out_t;
multi_thread_mem_stage_out_t multi_thread_mem_2_stages(
  uint8_t bid, // For sim print
  thread_context_t imem_in,
  thread_context_t dmem_in,
  riscv_mem_map_inputs_t mem_map_in[N_THREADS_PER_BARREL]
){
  multi_thread_mem_stage_out_t o;
  // Stage thread context, input from prev stages
  o.imem_out = imem_in;
  o.dmem_out = dmem_in;
  // Each thread has own instances of a shared instruction and data memory
  // Only current thread mem is enabled
  // Output from multiple threads is muxed
  uint32_t instructions[N_THREADS_PER_BARREL];
  uint32_t mem_rd_datas[N_THREADS_PER_BARREL];
  uint8_t tid;
  for(tid = 0; tid < N_THREADS_PER_BARREL; tid+=1)
  {
    uint32_t mem_addr;
    uint32_t mem_wr_data;
    uint1_t mem_wr_byte_ens[4];
    uint1_t mem_rd_byte_ens[4];
    mem_addr = dmem_in.exe.result; // addr always from execute module, not always used
    mem_wr_data = dmem_in.reg_file_rd_datas.rd_data2;
    mem_wr_byte_ens = dmem_in.decoded.mem_wr_byte_ens;
    mem_rd_byte_ens = dmem_in.decoded.mem_rd_byte_ens;
    // gate enables with thread id compare and valid
    if(!(dmem_in.thread_valid & (dmem_in.thread_id==tid))){
      mem_wr_byte_ens[0] = 0;
      mem_wr_byte_ens[1] = 0;
      mem_wr_byte_ens[2] = 0;
      mem_wr_byte_ens[3] = 0;
      mem_rd_byte_ens[0] = 0;
      mem_rd_byte_ens[1] = 0;
      mem_rd_byte_ens[2] = 0;
      mem_rd_byte_ens[3] = 0;
    }
    if(mem_wr_byte_ens[0]){
      printf("Barrel %d Thread %d: Write Mem[0x%X] = %d\n", bid, tid, mem_addr, mem_wr_data);
    }
    my_riscv_mem_out_t mem_out = my_riscv_mem(
      imem_in.pc>>2, // Instruction word read address based on PC
      mem_addr, // Main memory read/write address
      mem_wr_data, // Main memory write data
      mem_wr_byte_ens, // Main memory write data byte enables
      mem_rd_byte_ens // Main memory read data byte enables
      // Optional memory map inputs
      #ifdef riscv_mem_map_inputs_t
      , mem_map_in[tid]
      #endif
    );
    instructions[tid] = mem_out.inst;
    mem_rd_datas[tid] = mem_out.rd_data;
    if(mem_rd_byte_ens[0]){
      printf("Barrel %d Thread %d: Read Mem[0x%X] = %d\n", bid, tid, mem_addr, mem_out.rd_data);
    }
    o.mem_out_of_range[tid] = mem_out.mem_out_of_range;
    o.mem_map_out[tid] = mem_out.mem_map_outputs;
  }
  // Mux output based on current thread
  o.imem_out.instruction = instructions[imem_in.thread_id];
  o.dmem_out.mem_rd_data = mem_rd_datas[dmem_in.thread_id];
  return o;
}
#pragma MAIN mem_2_stages
void mem_2_stages()
{
  uint32_t bid;
  for (bid = 0; bid < N_BARRELS; bid+=1)
  {
    multi_thread_mem_stage_out_t multi_thread_mem_out = multi_thread_mem_2_stages(
      bid,
      imem_inputs[bid],
      dmem_inputs[bid],
      mem_map_inputs[bid]
    );
    imem_outputs[bid] = multi_thread_mem_out.imem_out;
    dmem_outputs[bid] = multi_thread_mem_out.dmem_out;
    mem_map_outputs[bid] = multi_thread_mem_out.mem_map_out;
    mem_out_of_range[bid] = multi_thread_mem_out.mem_out_of_range;
  }
}


#pragma MAIN decode_stages
void decode_stages()
{
  // Stage thread context, input from prev stage
  thread_context_t inputs[N_BARRELS] = decode_inputs;
  thread_context_t outputs[N_BARRELS] = inputs;

  uint32_t bid;
  for(bid = 0; bid < N_BARRELS; bid+=1)
  {
    if(inputs[bid].thread_valid){
      printf("Barrel %d Thread %d: Instruction: 0x%X\n", bid, inputs[bid].thread_id, inputs[bid].instruction);
      outputs[bid].decoded = decode(inputs[bid].instruction);
    }
    // Multiple unknown op outputs per thread
    uint8_t tid;
    for(tid = 0; tid < N_THREADS_PER_BARREL; tid+=1)
    {
      unknown_op[bid][tid] = 0;
      if(inputs[bid].thread_valid & (inputs[bid].thread_id==tid)){
        unknown_op[bid][tid] = outputs[bid].decoded.unknown_op;
      }
    }
  }
  // Output to next stage
  decode_outputs = outputs;
}


// Multiple copies of N thread register files
typedef struct multi_thread_reg_files_out_t{
  thread_context_t rd_outputs;
  thread_context_t wr_outputs;
}multi_thread_reg_files_out_t;
multi_thread_reg_files_out_t multi_thread_reg_files(
  uint8_t bid, // Barrel ID for sim printing
  thread_context_t rd_inputs,
  thread_context_t wr_inputs
){
  multi_thread_reg_files_out_t o;
  // Stage thread context, input from prev stages
  o.rd_outputs = rd_inputs;
  o.wr_outputs = wr_inputs;
  // Each thread has own instances of a shared instruction and data memory
  uint8_t tid;
  // Only current thread mem is enabled
  // Output from multiple threads is muxed
  reg_file_out_t reg_file_rd_datas[N_THREADS_PER_BARREL];
  for(tid = 0; tid < N_THREADS_PER_BARREL; tid+=1)
  {
    uint1_t wr_thread_sel = wr_inputs.thread_valid & (wr_inputs.thread_id==tid);
    // Register file reads and writes
    uint5_t reg_wr_addr;
    uint32_t reg_wr_data;
    uint1_t reg_wr_en;
    if(wr_thread_sel){
      // Reg file write back, drive inputs 
      reg_wr_en = wr_inputs.decoded.reg_wr;
      reg_wr_addr = wr_inputs.decoded.dest;
      // Determine data to write back
      reg_wr_data = select_reg_wr_data(
        wr_inputs.decoded,
        wr_inputs.exe,
        wr_inputs.mem_rd_data,
        wr_inputs.pc + 4
      );
    }
    reg_file_rd_datas[tid] = reg_file(
      rd_inputs.decoded.src1, // First read port address
      rd_inputs.decoded.src2, // Second read port address
      reg_wr_addr, // Write port address
      reg_wr_data, // Write port data
      reg_wr_en // Write enable
    );
    // Reg read
    uint1_t rd_thread_sel = rd_inputs.thread_valid & (rd_inputs.thread_id==tid);
    if(rd_thread_sel){
      if(rd_inputs.decoded.print_rs1_read){
        printf("Barrel %d Thread %d: Read RegFile[%d] = %d\n", bid, tid, rd_inputs.decoded.src1, reg_file_rd_datas[tid].rd_data1);
      }
      if(rd_inputs.decoded.print_rs2_read){
        printf("Barrel %d Thread %d: Read RegFile[%d] = %d\n", bid, tid, rd_inputs.decoded.src2, reg_file_rd_datas[tid].rd_data2);
      }
    }
  }
  // Mux output for selected thread
  o.rd_outputs.reg_file_rd_datas = reg_file_rd_datas[rd_inputs.thread_id];

  return o;
}
#pragma MAIN reg_file_2_stages
void reg_file_2_stages()
{
  uint32_t bid;
  for(bid = 0; bid < N_BARRELS; bid+=1)
  {
    multi_thread_reg_files_out_t reg_file_out = multi_thread_reg_files(
      bid, // Barrel ID for sim printing
      reg_rd_inputs[bid],
      reg_wr_inputs[bid]
    );
    reg_rd_outputs[bid] = reg_file_out.rd_outputs;
    reg_wr_outputs[bid] = reg_file_out.wr_outputs;
  }
}


#pragma MAIN execute_stages
void execute_stages()
{
  // Stage thread context, input from prev stage
  thread_context_t inputs[N_BARRELS] = exe_inputs;
  thread_context_t outputs[N_BARRELS] = inputs;
  uint32_t i;
  for(i = 0; i < N_BARRELS; i+=1)
  {
    // Execute stage
    uint32_t pc_plus4 = inputs[i].pc + 4;
    if(inputs[i].thread_valid){
      printf("Barrel %d Thread %d: Execute stage...\n", i, inputs[i].thread_id);
      outputs[i].exe = execute(
        inputs[i].pc, pc_plus4, 
        inputs[i].decoded, 
        inputs[i].reg_file_rd_datas.rd_data1, inputs[i].reg_file_rd_datas.rd_data2
      );
    }
  }
  // Output to next stage
  exe_outputs = outputs;
}


// Calculate the next PC
#pragma MAIN branch_next_pc_stages
void branch_next_pc_stages()
{
  // Stage thread context, input from prev stage
  thread_context_t inputs[N_BARRELS] = branch_inputs;
  thread_context_t outputs[N_BARRELS] = inputs;
  uint32_t i;
  for(i = 0; i < N_BARRELS; i+=1)
  {
    outputs[i].next_pc = 0;
    if(inputs[i].thread_valid){
      // Branch/Increment PC
      outputs[i].next_pc = select_next_pc(
        inputs[i].decoded,
        inputs[i].exe,
        inputs[i].pc + 4
      );
    }
  }
  // Output to next stage
  branch_outputs = outputs;
}
