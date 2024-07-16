#pragma once
#include "xstr.h" // xstr func for putting quotes around macro things

// This file includes preprocessor helper hacks to help with/work around
// simple single and dual port generated RAM template functions that exist: 
// https://github.com/JulianKemmerer/PipelineC/wiki/Automatically-Generated-Functionality#rams

// Ideally this could all be autogenerated: https://github.com/JulianKemmerer/PipelineC/issues/121

#define RAM_INIT_INT_ZEROS "(others => (others => '0'))"

// Dual port, one read+write, one read only, 0 latency
#define DECL_RAM_DP_RW_R_0( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  uint32_t addr0; \
  elem_t wr_data0; uint1_t wr_en0; \
  elem_t rd_data0; \
  uint1_t valid0; \
  uint32_t addr1; \
  elem_t rd_data1; \
  uint1_t valid1; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, \
  elem_t wr_data0, uint1_t wr_en0, \
  uint1_t valid0, \
  uint32_t addr1, \
  uint1_t valid1 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en0(0)='1' and valid0(0)='1' then \n\
          the_ram(addr0_s) <= wr_data0; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
  return_output.addr0 <= addr0; \n\
  return_output.rd_data0 <= the_ram(addr0_s); \n\
  return_output.wr_data0 <= wr_data0; \n\
  return_output.wr_en0 <= wr_en0; \n\
  return_output.valid0 <= valid0; \n\
  return_output.addr1 <= addr1; \n\
  return_output.rd_data1 <= the_ram(addr1_s); \n\
  return_output.valid1 <= valid1; \n\
"); \
}

// Triple port, one read+write, two read only, 0 latency
#define DECL_RAM_TP_RW_R_R_0( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  uint32_t addr0; \
  elem_t wr_data0; uint1_t wr_en0; \
  elem_t rd_data0; \
  uint1_t valid0; \
  uint32_t addr1; \
  elem_t rd_data1; \
  uint1_t valid1; \
  uint32_t addr2; \
  elem_t rd_data2; \
  uint1_t valid2; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, \
  elem_t wr_data0, uint1_t wr_en0, \
  uint1_t valid0, \
  uint32_t addr1, \
  uint1_t valid1, \
  uint32_t addr2, \
  uint1_t valid2 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr2_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr2_s <= to_integer(addr2(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en0(0)='1' and valid0(0)='1' then \n\
          the_ram(addr0_s) <= wr_data0; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
  return_output.addr0 <= addr0; \n\
  return_output.rd_data0 <= the_ram(addr0_s); \n\
  return_output.wr_data0 <= wr_data0; \n\
  return_output.wr_en0 <= wr_en0; \n\
  return_output.valid0 <= valid0; \n\
  return_output.addr1 <= addr1; \n\
  return_output.rd_data1 <= the_ram(addr1_s); \n\
  return_output.valid1 <= valid1; \n\
  return_output.addr2<= addr2; \n\
  return_output.rd_data2 <= the_ram(addr2_s); \n\
  return_output.valid2 <= valid2; \n\
"); \
}

// Triple port, one read+write, two read only, 1 clk latency
#define DECL_RAM_TP_RW_R_R_1( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  uint32_t addr0; \
  elem_t wr_data0; uint1_t wr_en0; \
  elem_t rd_data0; \
  uint1_t valid0; \
  uint32_t addr1; \
  elem_t rd_data1; \
  uint1_t valid1; \
  uint32_t addr2; \
  elem_t rd_data2; \
  uint1_t valid2; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, \
  elem_t wr_data0, uint1_t wr_en0, \
  uint1_t valid0, \
  uint1_t rd_en0, \
  uint32_t addr1, \
  uint1_t valid1, \
  uint1_t rd_en1, \
  uint32_t addr2, \
  uint1_t valid2, \
  uint1_t rd_en2 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr2_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr2_s <= to_integer(addr2(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en0(0)='1' and valid0(0)='1' then \n\
          the_ram(addr0_s) <= wr_data0; \n\
        end if; \n\
        if rd_en0(0) = '1' then \n\
          return_output.addr0 <= addr0; \n\
          return_output.rd_data0 <= the_ram(addr0_s); \n\
          return_output.wr_data0 <= wr_data0; \n\
          return_output.wr_en0 <= wr_en0; \n\
          return_output.valid0 <= valid0; \n\
        end if; \n\
        if rd_en1(0) = '1' then \n\
          return_output.addr1 <= addr1; \n\
          return_output.rd_data1 <= the_ram(addr1_s); \n\
          return_output.valid1 <= valid1; \n\
        end if; \n\
        if rd_en2(0) = '1' then \n\
          return_output.addr2<= addr2; \n\
          return_output.rd_data2 <= the_ram(addr2_s); \n\
          return_output.valid2 <= valid2; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}

// Single port with byte enables like for CPU
#define DECL_4BYTE_RAM_SP_RF_1(\
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_out_t \
{ \
  uint32_t addr0; \
  uint32_t wr_data0; uint1_t wr_byte_ens0[4]; \
  uint32_t rd_data0; \
  uint1_t valid0; \
}ram_name##_out_t; \
ram_name##_out_t ram_name( \
  uint32_t addr0,\
  uint32_t wr_data0, uint1_t wr_byte_ens0[4],\
  uint1_t valid0\
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of unsigned(31 downto 0); \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if valid0(0)='1' then \n\
          if wr_byte_ens0(0)(0)='1'then \n\
            the_ram(addr0_s)(7 downto 0) <= wr_data0(7 downto 0); \n\
          end if;  \n\
          if wr_byte_ens0(1)(0)='1'then \n\
            the_ram(addr0_s)(15 downto 8) <= wr_data0(15 downto 8); \n\
          end if;  \n\
          if wr_byte_ens0(2)(0)='1'then \n\
            the_ram(addr0_s)(23 downto 16) <= wr_data0(23 downto 16); \n\
          end if;  \n\
          if wr_byte_ens0(3)(0)='1'then \n\
            the_ram(addr0_s)(31 downto 24) <= wr_data0(31 downto 24); \n\
          end if;  \n\
        end if; \n\
        return_output.addr0 <= addr0; \n\
        return_output.rd_data0 <= the_ram(addr0_s); \n\
        return_output.wr_data0 <= wr_data0; \n\
        return_output.wr_byte_ens0 <= wr_byte_ens0; \n\
        return_output.valid0 <= valid0; \n\
      end if; \n\
    end if; \n\
  end process; \n\
");\
}

// Dual port, one read+write, one read only, 1 clock latency
#define DECL_RAM_DP_RW_R_1( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  uint32_t addr0; elem_t wr_data0; uint1_t wr_en0; \
  elem_t rd_data0; \
  uint1_t valid0; \
  uint32_t addr1; \
  elem_t rd_data1; \
  uint1_t valid1; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, elem_t wr_data0, uint1_t wr_en0, \
  uint1_t valid0, \
  uint1_t rd_en0, \
  uint32_t addr1, \
  uint1_t valid1, \
  uint1_t rd_en1 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en0(0) = '1' and valid0(0)='1' then \n\
          the_ram(addr0_s) <= wr_data0; \n\
        end if; \n\
        if rd_en0(0) = '1' then \n\
          return_output.addr0 <= addr0; \n\
          return_output.wr_data0 <= wr_data0; \n\
          return_output.wr_en0 <= wr_en0; \n\
          return_output.rd_data0 <= the_ram(addr0_s); \n\
          return_output.valid0 <= valid0; \n\
        end if; \n\
        if rd_en1(0) = '1' then \n\
          return_output.addr1 <= addr1; \n\
          return_output.rd_data1 <= the_ram(addr1_s); \n\
          return_output.valid1 <= valid1; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}

// Very dumb copy of above with port order switched :(?
#define DECL_RAM_DP_R_RW_1( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  uint32_t addr0; \
  elem_t rd_data0; \
  uint1_t valid0; \
  uint32_t addr1; elem_t wr_data1; uint1_t wr_en1; \
  elem_t rd_data1; \
  uint1_t valid1; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, \
  uint1_t valid0, \
  uint1_t rd_en0, \
  uint32_t addr1, elem_t wr_data1, uint1_t wr_en1, \
  uint1_t valid1, \
  uint1_t rd_en1 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if rd_en0(0) = '1' then \n\
          return_output.addr0 <= addr0; \n\
          return_output.rd_data0 <= the_ram(addr0_s); \n\
          return_output.valid0 <= valid0; \n\
        end if; \n\
        if wr_en1(0) = '1' and valid1(0)='1' then \n\
          the_ram(addr1_s) <= wr_data1; \n\
        end if; \n\
        if rd_en1(0) = '1' then \n\
          return_output.addr1 <= addr1; \n\
          return_output.wr_data1 <= wr_data1; \n\
          return_output.wr_en1 <= wr_en1; \n\
          return_output.rd_data1 <= the_ram(addr1_s); \n\
          return_output.valid1 <= valid1; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}

// Dual port, two read+write ports, 1 clock latency
// Does not have read enable separate from clock enable
#define DECL_RAM_DP_RW_RW_1( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  uint32_t addr0; elem_t wr_data0; uint1_t wr_en0; \
  elem_t rd_data0; \
  uint1_t valid0; \
  uint32_t addr1; elem_t wr_data1; uint1_t wr_en1; \
  elem_t rd_data1; \
  uint1_t valid1; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, elem_t wr_data0, uint1_t wr_en0, \
  uint1_t valid0, \
  uint32_t addr1, elem_t wr_data1, uint1_t wr_en1, \
  uint1_t valid1 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  shared variable the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        return_output.valid0 <= valid0; \n\
        return_output.addr0 <= addr0; \n\
        return_output.wr_data0 <= wr_data0; \n\
        return_output.wr_en0 <= wr_en0; \n\
        return_output.rd_data0 <= the_ram(addr0_s); \n\
        if wr_en0(0) = '1' and valid0(0)='1' then \n\
          the_ram(addr0_s) := wr_data0; \n\
        end if; \n\
      end if; \n\
    end if; \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        return_output.valid1 <= valid1; \n\
        return_output.addr1 <= addr1; \n\
        return_output.wr_data1 <= wr_data1; \n\
        return_output.wr_en1 <= wr_en1; \n\
        return_output.rd_data1 <= the_ram(addr1_s); \n\
        if wr_en1(0) = '1' and valid1(0)='1' then \n\
          the_ram(addr1_s) := wr_data1; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}

// Dual port, one read+write, one read only
// 2 clock latency from input and output regs
#define DECL_RAM_DP_RW_R_2( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  elem_t rd_data0; \
  elem_t rd_data1; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, elem_t wr_data0, uint1_t wr_en0, \
  uint1_t en0, uint1_t rd_en0, \
  uint32_t addr1, \
  uint1_t en1, uint1_t rd_en1 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal wr_data0_r : " xstr(elem_t) "; \n\
  signal wr_en0_r : std_logic; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if en0(0) = '1' then \n\
          addr0_s <= to_integer(addr0(30 downto 0)) \n\
          -- synthesis translate_off \n\
          mod SIZE \n\
          -- synthesis translate_on \n\
          ; \n\
          wr_data0_r <= wr_data0; \n\
          wr_en0_r <= wr_en0(0); \n\
        end if; \n\
        if en1(0) = '1' then \n\
          addr1_s <= to_integer(addr1(30 downto 0)) \n\
          -- synthesis translate_off \n\
          mod SIZE \n\
          -- synthesis translate_on \n\
          ; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en0_r = '1' then \n\
          the_ram(addr0_s) <= wr_data0_r; \n\
        end if; \n\
        if rd_en0(0) = '1' then \n\
          return_output.rd_data0 <= the_ram(addr0_s); \n\
        end if; \n\
        if rd_en1(0) = '1' then \n\
          return_output.rd_data1 <= the_ram(addr1_s); \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}

// Dual port, one read+write, one read only
// 2 clock latency from two sets of output regs
#define DECL_RAM_DP_RW_R_2_O( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_outputs_t \
{ \
  elem_t rd_data0; \
  elem_t rd_data1; \
}ram_name##_outputs_t; \
ram_name##_outputs_t ram_name( \
  uint32_t addr0, elem_t wr_data0, uint1_t wr_en0, \
  uint1_t en0, uint1_t rd_en0, \
  uint32_t addr1, \
  uint1_t en1, uint1_t rd_en1 \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal addr1_s : integer range 0 to SIZE-1 := 0; \n\
  signal rd_data0_r : " xstr(elem_t) "; \n\
  signal rd_data1_r : " xstr(elem_t) "; \n\
begin \n\
  process(all) begin \n\
    addr0_s <= to_integer(addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    addr1_s <= to_integer(addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) is \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        rd_data0_r <= the_ram(addr0_s); \n\
        if rd_en0(0) = '1' then \n\
          return_output.rd_data0 <= rd_data0_r; \n\
        end if; \n\
        rd_data1_r <= the_ram(addr1_s); \n\
        if rd_en1(0) = '1' then \n\
          return_output.rd_data1 <= rd_data1_r; \n\
        end if; \n\
        if wr_en0(0) = '1' then \n\
          the_ram(addr0_s) <= wr_data0; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}

// Triple port, two read only, one write only, 0 latency
#define DECL_RAM_TP_R_R_W_0( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_out_t \
{ \
  elem_t rd_data0; \
  elem_t rd_data1; \
}ram_name##_out_t; \
ram_name##_out_t ram_name( \
  uint32_t rd_addr0, \
  uint32_t rd_addr1, \
  uint32_t wr_addr, elem_t wr_data, uint1_t wr_en \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal rd_addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal rd_addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    rd_addr0_s <= to_integer(rd_addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    rd_addr1_s <= to_integer(rd_addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en(0) = '1' then \n\
          the_ram(to_integer(wr_addr)) <= wr_data; \n\
        end if; \n\
      end if; \n\
    end if; \n\
  end process; \n\
  return_output.rd_data0 <= the_ram(rd_addr0_s); \n\
  return_output.rd_data1 <= the_ram(rd_addr1_s); \n\
"); \
}


// Triple port, two read only, one write only, 1 latency
#define DECL_RAM_TP_R_R_W_1( \
  elem_t, \
  ram_name, \
  SIZE, \
  VHDL_INIT \
) \
typedef struct ram_name##_out_t \
{ \
  elem_t rd_data0; \
  elem_t rd_data1; \
}ram_name##_out_t; \
ram_name##_out_t ram_name( \
  uint32_t rd_addr0, \
  uint32_t rd_addr1, \
  uint32_t wr_addr, elem_t wr_data, uint1_t wr_en \
){ \
  __vhdl__("\n\
  constant SIZE : integer := " xstr(SIZE) "; \n\
  type ram_t is array(0 to SIZE-1) of " xstr(elem_t) "; \n\
  signal the_ram : ram_t := " VHDL_INIT "; \n\
  -- Limit zero latency comb. read addr range to SIZE \n\
  -- since invalid addresses can occur as logic propogates \n\
  -- (this includes out of int32 range u32 values) \n\
  signal rd_addr0_s : integer range 0 to SIZE-1 := 0; \n\
  signal rd_addr1_s : integer range 0 to SIZE-1 := 0; \n\
begin \n\
  process(all) begin \n\
    rd_addr0_s <= to_integer(rd_addr0(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
    rd_addr1_s <= to_integer(rd_addr1(30 downto 0)) \n\
    -- synthesis translate_off \n\
    mod SIZE \n\
    -- synthesis translate_on \n\
    ; \n\
  end process; \n\
  process(clk) \n\
  begin \n\
    if rising_edge(clk) then \n\
      if CLOCK_ENABLE(0)='1' then \n\
        if wr_en(0) = '1' then \n\
          the_ram(to_integer(wr_addr)) <= wr_data; \n\
        end if; \n\
        return_output.rd_data0 <= the_ram(rd_addr0_s); \n\
        return_output.rd_data1 <= the_ram(rd_addr1_s); \n\
      end if; \n\
    end if; \n\
  end process; \n\
"); \
}
