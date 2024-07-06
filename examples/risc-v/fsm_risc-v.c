// RISCV CPU, FSM that takes multiple cycles
// uses autopipelines for some stages

#pragma PART "xc7a35ticsg324-1l" //LFE5U-85F-6BG381C"
#include "uintN_t.h"
#include "intN_t.h"

// Include test gcc compiled program
#include "gcc_test/mem_map.h" 
#include "gcc_test/text_mem_init.h"
#include "gcc_test/data_mem_init.h"

// Helpers macros for building mmio modules
#include "mem_map.h" 
// Define MMIO inputs and outputs
typedef struct my_mmio_in_t{
  uint1_t button;
}my_mmio_in_t;
typedef struct my_mmio_out_t{
  //uint32_t return_value;
  //uint1_t halt;
  uint1_t led;
}my_mmio_out_t;
// Define the hardware memory for those IO
RISCV_DECL_MEM_MAP_MOD_OUT_T(my_mmio_out_t)
riscv_mem_map_mod_out_t(my_mmio_out_t) my_mem_map_module(
  RISCV_MEM_MAP_MOD_INPUTS(my_mmio_in_t)
){
  // Outputs
  static riscv_mem_map_mod_out_t(my_mmio_out_t) o_reg;
  riscv_mem_map_mod_out_t(my_mmio_out_t) o = o_reg;
  o.addr_is_mapped = 0; // since o is static regs
  // Memory muxing/select logic
  // Uses helper comparing word address and driving a variable
  //WORD_MM_ENTRY(o, THREAD_ID_RETURN_OUTPUT_ADDR, o.outputs.return_value)
  //o.outputs.halt = wr_byte_ens[0] & (addr==THREAD_ID_RETURN_OUTPUT_ADDR);
  WORD_MM_ENTRY_NEW(LED_ADDR, o_reg.outputs.led, o_reg.outputs.led, addr, o.addr_is_mapped, o.rd_data)
  return o;
}

// RISC-V components
#define RISCV_REGFILE_1_CYCLE
#include "risc-v.h"

// Declare instruction and data memory
// also includes memory mapped IO
#define RISCV_IMEM_INIT         text_MEM_INIT // from gcc_test/
#define RISCV_IMEM_SIZE_BYTES   IMEM_SIZE     // from gcc_test/
#define RISCV_DMEM_INIT         data_MEM_INIT // from gcc_test/
#define RISCV_DMEM_SIZE_BYTES   DMEM_SIZE     // from gcc_test/
#define riscv_mem_map           my_mem_map_module
#define riscv_mem_map_inputs_t  my_mmio_in_t
#define riscv_mem_map_outputs_t my_mmio_out_t
#define RISCV_IMEM_1_CYCLE
#define RISCV_DMEM_1_CYCLE
// Multi cycle is not a pipeline
#define RISCV_IMEM_NO_AUTOPIPELINE
#define RISCV_DMEM_NO_AUTOPIPELINE
#include "mem_decl.h"

// Declare a globally visible auto pipeline out of exe logic
#include "global_func_inst.h"
GLOBAL_PIPELINE_INST_W_VALID_ID(execute_rv32_m_ext_pipeline, execute_t, execute_rv32_m_ext, execute_rv32_m_ext_in_t) 

typedef enum cpu_state_t{
  // Use PC as addr into IMEM
  FETCH_START, 
  // Get instruction from IMEM, decode it, supply regfile read addresses
  FETCH_END_DECODE_REG_RD_START,
  // Use read data from regfile for rv32i execute, output of rv32i execute into DMEM
  REG_RD_END_EXE_RV32I_MEM_START,
  // Use read data from regfile for rv32_m_ext execute start
  REG_RD_END_M_EXE_START,
  // Wait for output data from rv32_m_ext execute, data into dmem
  M_EXE_END_MEM_START,
  // Use DMEM read data for doing write back, update to next pc
  MEM_END_WRITE_BACK_NEXT_PC
}cpu_state_t;

// CPU top level
typedef struct riscv_out_t{
  // Debug IO
  uint1_t halt;
  uint32_t return_value;
  uint32_t pc;
  uint1_t unknown_op;
  uint1_t mem_out_of_range;
  riscv_mem_map_outputs_t mem_map_outputs;
}riscv_out_t;
riscv_out_t fsm_riscv(
  riscv_mem_map_inputs_t mem_map_inputs
)
{
  // Top level outputs
  riscv_out_t o;

  // CPU multi cycle state reg
  static cpu_state_t state = FETCH_START;
  cpu_state_t next_state = state; // Update static reg at end of func
  
  // CPU registers (outside of reg file)
  static uint32_t pc = 0;
  o.pc = pc; // debug
  //  Wires/non-static local variables from original single cycle design
  //  are likely to be shared/stored from cycle to cycle now
  //  so just declare all (non-FEEDBACK) local variables static registers too
  static uint32_t pc_plus4_reg;
  static riscv_imem_ram_out_t imem_out_reg;
  static decoded_t decoded_reg;
  static reg_file_out_t reg_file_out_reg;
  static execute_t exe_reg;
  static uint1_t mem_wr_byte_ens_reg[4];
  static uint1_t mem_rd_byte_ens_reg[4];
  static uint32_t mem_addr_reg;
  static uint32_t mem_wr_data_reg;
  static riscv_dmem_out_t dmem_out_reg;
  // But still have non-static wires version of registers too
  // to help avoid/resolve unintentional passing of data between stages
  // in the same cycle (like single cycle cpu did)
  uint32_t pc_plus4 = pc_plus4_reg;
  riscv_imem_ram_out_t imem_out = imem_out_reg;
  decoded_t decoded = decoded_reg;
  reg_file_out_t reg_file_out = reg_file_out_reg;
  execute_t exe = exe_reg;
  uint1_t mem_wr_byte_ens[4] = mem_wr_byte_ens_reg;
  uint1_t mem_rd_byte_ens[4] = mem_rd_byte_ens_reg;
  uint32_t mem_addr = mem_addr_reg;
  uint32_t mem_wr_data = mem_wr_data_reg;
  riscv_dmem_out_t dmem_out = dmem_out_reg;
  
  // PC fetch
  if((state==FETCH_START)){
    printf("PC FETCH_START\n");
    // Program counter is input to IMEM
    pc_plus4 = pc + 4;
    printf("PC = 0x%X\n", pc);
    next_state = FETCH_END_DECODE_REG_RD_START;
  }

  // Boundary shared between cycle0 and cycle1
  // Instruction memory
  imem_out = riscv_imem_ram(pc>>2, 1);

  // Decode
  if((state==FETCH_END_DECODE_REG_RD_START)){
    printf("Decode:\n");
    // Decode the instruction to control signals
    printf("Instruction: 0x%X\n", imem_out.rd_data1);
    decoded = decode(imem_out.rd_data1);
    o.unknown_op = decoded.unknown_op; // debug
    if(decoded.is_rv32_m_ext){
      // Multi cycle exe version for rv32mi
      next_state = REG_RD_END_M_EXE_START;
    }else{
      // Regular rv32i
      next_state = REG_RD_END_EXE_RV32I_MEM_START;
    }
  }

  // Register file reads and writes
  //  Register file write signals are not driven until later in code
  //  but are used now, requiring FEEDBACK pragma
  uint5_t reg_wr_addr;
  uint32_t reg_wr_data;
  uint1_t reg_wr_en;
  #pragma FEEDBACK reg_wr_addr
  #pragma FEEDBACK reg_wr_data
  #pragma FEEDBACK reg_wr_en
  reg_file_out = reg_file(
    decoded.src1, // First read port address
    decoded.src2, // Second read port address
    reg_wr_addr, // Write port address
    reg_wr_data, // Write port data
    reg_wr_en // Write enable
  );
  if((state==REG_RD_END_EXE_RV32I_MEM_START)|(state==REG_RD_END_M_EXE_START)){
    if(decoded_reg.print_rs1_read){
      printf("Read RegFile[%d] = %d\n", decoded_reg.src1, reg_file_out.rd_data1);
    }
    if(decoded_reg.print_rs2_read){
      printf("Read RegFile[%d] = %d\n", decoded_reg.src2, reg_file_out.rd_data2);
    }
  }
  
  // RV32 M MUL+DIV execute start
  execute_rv32_m_ext_pipeline_in_valid = 0; // Default no data into pipeline
  if(state==REG_RD_END_M_EXE_START){
    printf("RV32 M Execute Start:\n");
    // Put valid data input into pipeline
    execute_rv32_m_ext_pipeline_in.decoded = decoded_reg;
    execute_rv32_m_ext_pipeline_in.reg1 = reg_file_out.rd_data1;
    execute_rv32_m_ext_pipeline_in.reg2 = reg_file_out.rd_data2;
    execute_rv32_m_ext_pipeline_in_valid = 1;
    // And start waiting for a valid output
    next_state = M_EXE_END_MEM_START;
  }
  // RV32I Execute
  if((state==REG_RD_END_EXE_RV32I_MEM_START)){
    printf("RV32I Execute:\n");
    exe = execute(
      pc, pc_plus4_reg, 
      decoded_reg, 
      reg_file_out.rd_data1, reg_file_out.rd_data2
    );
    next_state = MEM_END_WRITE_BACK_NEXT_PC;
  }
  // RV32 M MUL+DIV execute finish
  if(state==M_EXE_END_MEM_START){
    // Wait for valid output from pipeline
    printf("Waiting for RV32 M Execute to end...\n");
    if(execute_rv32_m_ext_pipeline_out_valid){
      printf("RV32 M Execute End:\n");
      // Then move on to next state
      // (mem inputs wired below)
      next_state = MEM_END_WRITE_BACK_NEXT_PC;
    }
  }

  // Boundary shared between exe and dmem
  // Drive dmem inputs:
  // Default no writes or reads
  ARRAY_SET(mem_wr_byte_ens, 0, 4)
  ARRAY_SET(mem_rd_byte_ens, 0, 4)
  mem_addr = 0;
  mem_wr_data = 0;
  if((state==M_EXE_END_MEM_START)){
    // TODO do M ext instructions ever set mem_wr/rd_byte_ens?
    // is below not needed? just wait for pipeline results 
    // Exe result address from pipeline
    mem_addr = execute_rv32_m_ext_pipeline_out.result;
    // data from regfile out reg held from prev cycles
    mem_wr_data = reg_file_out_reg.rd_data2;
    // Wait to start write or read en during first cycle of two cycle read
    // Which is cycle when execute result comes out of pipeline
    if(execute_rv32_m_ext_pipeline_out_valid){
      mem_wr_byte_ens = decoded_reg.mem_wr_byte_ens;
      mem_rd_byte_ens = decoded_reg.mem_rd_byte_ens;
    }
  }
  if((state==REG_RD_END_EXE_RV32I_MEM_START)){
    // addr and data from same cycle rv32i execute module and regfile out
    mem_addr = exe.result; 
    mem_wr_data = reg_file_out.rd_data2;
    // Only write or read en during first cycle of two cycle read
    mem_wr_byte_ens = decoded_reg.mem_wr_byte_ens;
    mem_rd_byte_ens = decoded_reg.mem_rd_byte_ens;
  }
  if(mem_wr_byte_ens[0]){
    printf("Write Mem[0x%X] = %d\n", mem_addr, mem_wr_data);
  }
  // DMEM always "in use" regardless of stage
  // since memory map IO need to be connected always
  dmem_out = riscv_dmem(
    mem_addr, // Main memory read/write address
    mem_wr_data, // Main memory write data
    mem_wr_byte_ens, // Main memory write data byte enables
    mem_rd_byte_ens // Main memory read enable
    // Optional memory map inputs
    #ifdef riscv_mem_map_inputs_t
    , mem_map_inputs
    #endif
  );
  o.mem_out_of_range = dmem_out.mem_out_of_range; // debug
  // Optional outputs from memory map
  #ifdef riscv_mem_map_outputs_t
  o.mem_map_outputs = dmem_out.mem_map_outputs;
  #endif
  // Data memory outputs in stage3
  if((state==MEM_END_WRITE_BACK_NEXT_PC)){
    // Read output available from dmem_out in second cycle of two cycle read
    if(decoded_reg.mem_rd_byte_ens[0]){
      printf("Read Mem[0x%X] = %d\n", mem_addr_reg, dmem_out.rd_data);
    }
  }

  // Write Back + Next PC
  // default values needed for feedback signals
  reg_wr_en = 0; // default no writes 
  reg_wr_addr = 0;
  reg_wr_data = 0;
  if((state==MEM_END_WRITE_BACK_NEXT_PC)){
    printf("Write Back + Next PC:\n");
    // Reg file write back, drive inputs (FEEDBACK)
    reg_wr_en = decoded_reg.reg_wr;
    reg_wr_addr = decoded_reg.dest;
    // Determine reg data to write back (sign extend etc)
    reg_wr_data = select_reg_wr_data(
      decoded_reg,
      exe_reg,
      dmem_out.rd_data,
      pc_plus4_reg
    );
    // Branch/Increment PC
    pc = select_next_pc(decoded_reg, exe_reg, pc_plus4_reg);
    next_state = FETCH_START;
  }

  // Reg update
  state = next_state;
  pc_plus4_reg = pc_plus4;
  imem_out_reg = imem_out;
  decoded_reg = decoded;
  reg_file_out_reg = reg_file_out;
  exe_reg = exe;
  mem_wr_byte_ens_reg = mem_wr_byte_ens;
  mem_rd_byte_ens_reg = mem_rd_byte_ens;
  mem_addr_reg = mem_addr;
  mem_wr_data_reg = mem_wr_data;
  dmem_out_reg = dmem_out;
  
  return o;
}

// LEDs for demo
#include "leds/leds_port.c"

#pragma MAIN_MHZ my_top 60.0
void my_top()
{
  // Instance of core
  my_mmio_in_t in; // Disconnected for now
  riscv_out_t out = fsm_riscv(in);

  // Sim debug
  //unknown_op = out.unknown_op;
  //mem_out_of_range = out.mem_out_of_range;
  //halt = out.mem_map_outputs.halt;
  //main_return = out.mem_map_outputs.return_value;

  // Output LEDs for hardware debug
  static uint1_t mem_out_of_range;
  static uint1_t unknown_op;
  leds = 0;
  leds |= (uint4_t)out.mem_map_outputs.led << 0;
  leds |= (uint4_t)mem_out_of_range << 1;
  leds |= (uint4_t)unknown_op << 2;

  // Sticky on so human can see single cycle pulse
  mem_out_of_range |= out.mem_out_of_range;
  unknown_op |= out.unknown_op;
}
