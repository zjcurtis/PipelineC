#include "wire.h"
#include "uintN_t.h"
#include "arrays.h"
#include "../leds/leds.c"
// Include I2S from PMOD
#include "i2s_pmod.c"

// Do small buffered loopback
// Do I2S->AXIS loopback
// Do basic I2S->AXIS-> higher clock for fancier DSP

/*
https://reference.digilentinc.com/pmod/pmodi2s2/reference-manual
  Quick Start
  To set up a simple 44.1 kHz audio passthrough, three control signals need to be generated by the host system board.
  1. A master clock (MCLK) at a frequency of approximately 22.579 MHz.
  2. A serial clock (SCLK), which fully toggles once per 8 MCLK periods.
  3. A Left/Right Word Select signal, which fully toggles once per 64 SCLK periods.
  The Pmod I2S2's Master/Slave select jumper (JP1) should be placed into the Slave (SLV) position.
  Each of these control signals should be provided to the appropriate pin on both the top and bottom rows of the Pmod I2S2.
  The ADOUT_SDIN pin should be driven by the ADIN_SDOUT signal.
*/
// This configuration does not exactly match I2S spec, which allows for back to back sample data
// However, in this case, as seen in Digilent docs, sample data is 
// padded with trailing zeros for several cycles before the next channel sample begins
#pragma MAIN_MHZ app 22.579
#define SCLK_PERIOD_MCLKS 8
#define LR_PERIOD_SCLKS 64
#define LEFT 0
#define RIGHT 1
#define SAMPLE_BITWIDTH 24
#define sample_t uint24_t
#define bits_to_sample uint1_array24_le

// I2S stereo sample types
typedef struct i2s_samples_t
{
  sample_t l_data;
  sample_t r_data;
}i2s_samples_t;
// _s 'stream' of the above data
typedef struct i2s_samples_s
{
  i2s_samples_t samples;
  uint1_t valid;
}i2s_samples_s;

// Both RX and TX are dealing with the I2S protocol
typedef enum i2s_state_t
{
  RL_WAIT, // Sync up to the R->L I2S start of frame, left sample data
  LR_WAIT, // Sync up to the L->R start of right sample data
  SAMPLE, // Bits for a sample
}i2s_state_t;

// Logic to receive I2S stereo samples
i2s_samples_s i2s_rx(uint1_t data, uint1_t lr, uint1_t sclk_rising_edge, uint1_t reset_n)
{
  // Registers
  static i2s_state_t state;
  static uint1_t last_lr;
  static uint1_t curr_sample_bits[SAMPLE_BITWIDTH];
  static uint5_t curr_sample_bit_count;
  static i2s_samples_s output_reg;
  static uint1_t l_sample_valid;
  static uint1_t r_sample_valid;
  
  // Defaults
  i2s_samples_s rv = output_reg; // Output reg directly drives return value
  output_reg.valid = 0; // No outgoing samples
  
  // FSM Logic:
  
  // Everything occuring relative to sclk rising edges
  if(sclk_rising_edge)
  {
    // Waiting for R->L transition start of I2S frame, left sample data
    if(state==RL_WAIT)
    {
      if((last_lr==RIGHT) & (lr==LEFT))
      {
        // Next SCLK rising edge starts sample bits
        state = SAMPLE;
      }
    }
    // Waiting for L->R transition start of right sample data
    else if(state==LR_WAIT)
    {
      if((last_lr==LEFT) & (lr==RIGHT))
      {
        // Next SCLK rising edge starts sample bits
        state = SAMPLE;
      }
    }
    // Sampling data bit
    else // if(state==SAMPLE)
    {
      // Data is MSB first, put data into bottom/lsb such that
      // after N bits the first bit is in correct positon at MSB
      ARRAY_SHIFT_BIT_INTO_BOTTOM(curr_sample_bits, SAMPLE_BITWIDTH, data)
      
      // Was this the last bit?
      if(curr_sample_bit_count==(SAMPLE_BITWIDTH-1))
      {
        curr_sample_bit_count = 0; // Reset count
        sample_t curr_sample = bits_to_sample(curr_sample_bits);
        // Were these bits L or R data?
        // Last LR since last bit LR switched at falling edge for next channel potentially
        if(last_lr==LEFT) 
        {
          output_reg.samples.l_data = curr_sample;
          l_sample_valid = 1;
          // Right sample next
          state = LR_WAIT;
        }
        else
        {
          output_reg.samples.r_data = curr_sample;
          r_sample_valid = 1;
          // Left sample next
          state = RL_WAIT;
        }
        
        // Enough to output stream data of both LR samples?
        if(l_sample_valid & r_sample_valid)
        {
          output_reg.valid = 1;
          // Clear individual LR valid for next time
          l_sample_valid = 0;
          r_sample_valid = 0;
        }
      }
      else
      {
        // More bits coming
        curr_sample_bit_count += 1;
      }
    }    
    
    // Remember lr value for next iter
    last_lr = lr;
  }
  
  // Resets
  if(!reset_n)
  {
    state = RL_WAIT;
    last_lr = lr;
    curr_sample_bit_count = 0;
    l_sample_valid = 0;
    r_sample_valid = 0;
  }
  
  return rv;
}

// Logic to transmit I2S samples
typedef struct i2s_tx_t
{
  uint1_t samples_ready;
  uint1_t data;
}i2s_tx_t;
i2s_tx_t i2s_tx(i2s_samples_s samples, uint1_t lr, uint1_t sclk_falling_edge, uint1_t reset_n)
{
  // Registers
  static i2s_state_t state;
  static uint1_t last_lr;
  static uint1_t curr_sample_bits[SAMPLE_BITWIDTH];
  static uint5_t curr_sample_bit_count;
  static i2s_samples_s input_reg;
  static uint1_t l_sample_done;
  static uint1_t r_sample_done;
  static uint1_t output_data_reg;
  
  // Defaults
  i2s_tx_t rv;
  rv.data = output_data_reg;
  rv.samples_ready = !input_reg.valid; // Ready for new samples if have none
  
  // FSM Logic:
  
  // Everything occuring relative to sclk falling edges
  if(sclk_falling_edge)
  {
    // Waiting for R->L transition start of I2S frame, left sample data
    if(state==RL_WAIT)
    {
      output_data_reg = 0; // No out data until next sample
      if((last_lr==RIGHT) & (lr==LEFT))
      {
        // Next SCLK rising edge starts sample bits
        state = SAMPLE;
        // Make sure output reg has value
        sample_t curr_sample = 0;
        if(input_reg.valid)
        {
          curr_sample = input_reg.samples.l_data;
        }
        UINT_TO_BIT_ARRAY(curr_sample_bits, SAMPLE_BITWIDTH, curr_sample)
        // Data is MSB first, output bit from top of array
        output_data_reg = curr_sample_bits[SAMPLE_BITWIDTH-1];
      }
    }
    // Waiting for L->R transition start of right sample data
    else if(state==LR_WAIT)
    {
      output_data_reg = 0; // No out data until next sample
      if((last_lr==LEFT) & (lr==RIGHT))
      {
        // Next SCLK rising edge starts sample bits
        state = SAMPLE;
        // Make sure output reg has value
        sample_t curr_sample = 0;
        if(input_reg.valid)
        {
          curr_sample = input_reg.samples.r_data;
        }
        UINT_TO_BIT_ARRAY(curr_sample_bits, SAMPLE_BITWIDTH, curr_sample)
        // Data is MSB first, output bit from top of array
        output_data_reg = curr_sample_bits[SAMPLE_BITWIDTH-1];
      }
    }
    // Transmitting sample data bits
    else // if(state==SAMPLE)
    {
      // Current bit is output is from reg above
      // Then shift bits up and out for next bit in MSB
      ARRAY_SHIFT_UP(curr_sample_bits, SAMPLE_BITWIDTH, 1)
      // Next data output bit from top of array
      output_data_reg = curr_sample_bits[SAMPLE_BITWIDTH-1];
      
      // Was this the last bit?
      if(curr_sample_bit_count==(SAMPLE_BITWIDTH-1))
      {
        curr_sample_bit_count = 0; // Reset count
        // Were these bits L or R data?
        // Last LR since last bit LR switched at falling edge for next channel potentially
        if(last_lr==LEFT)
        {
          l_sample_done = 1;
          // Right sample next
          state = LR_WAIT;
        }
        else
        {
          r_sample_done = 1;
          // Left sample next
          state = RL_WAIT;
        }
        
        // Done outputting both LR samples?
        if(l_sample_done & r_sample_done)
        {
          // Clear input reg
          input_reg.valid = 0;
          // Clear individual LR done for next time
          l_sample_done = 0;
          r_sample_done = 0;
        }
      }
      else
      {
        // More bits coming
        curr_sample_bit_count += 1;
      }
    }    
    
    // Remember lr value for next iter
    last_lr = lr;
  }
  
  // Data into input reg when ready
  if(rv.samples_ready)
  {
    input_reg = samples;
  }
  
  // Resets
  if(!reset_n)
  {
    state = RL_WAIT;
    last_lr = lr;
    curr_sample_bit_count = 0;
    input_reg.valid = 0;
    l_sample_done = 0;
    r_sample_done = 0;
    output_data_reg = 0;
  }
  
  return rv;
}

void app(uint1_t reset_n)
{
  // Registers
  static uint1_t overflow;
  static uint3_t sclk_counter;
  static uint1_t sclk;
  static uint6_t lr_counter;
  static uint1_t lr;
  
  // Read the incoming I2S signals
  i2s_to_app_t from_i2s = read_i2s_pmod();
  
  // Outgoing I2S signals
  app_to_i2s_t to_i2s;
  
  // Outputs clks from registers
  to_i2s.tx_sclk = sclk;
  to_i2s.rx_sclk = sclk;
  to_i2s.tx_lrck = lr;
  to_i2s.rx_lrck = lr;
  
  // SCLK toggling at half period count on MCLK rising edge
  uint1_t sclk_half_toggle = sclk_counter==((SCLK_PERIOD_MCLKS/2)-1);
  uint1_t sclk_rising_edge = sclk_half_toggle & (sclk==0);
  uint1_t sclk_falling_edge = sclk_half_toggle & (sclk==1);
  
  // Receive I2S samples
  i2s_samples_s rx_samples = i2s_rx(from_i2s.rx_data, lr, sclk_rising_edge, reset_n);
  
  // Transmit I2S samples
  i2s_tx_t tx = i2s_tx(rx_samples, lr, sclk_falling_edge, reset_n);
  to_i2s.tx_data = tx.data;
  
  // Detect overflow if had receive data incoming but transmit wasnt ready
  WIRE_WRITE(uint4_t, leds, uint1_4(overflow)) // Light up LEDs 0-3 if overflow
  if(rx_samples.valid & !tx.samples_ready)
  {
    overflow = 1;
  }
  
  // Drive I2S clocking derived from current MCLK domain
  if(sclk_half_toggle)
  {
    // Do toggle and reset counter
    sclk = !sclk;
    sclk_counter = 0;
  }
  else
  {
    // No toggle yet, keep counting
    sclk_counter += 1;
  }
  
  // LR counting SCLK falling edges
  if(sclk_falling_edge)
  {
    // LR toggle at half period count 
    if(lr_counter==((LR_PERIOD_SCLKS/2)-1))
    {
      // Do toggle and reset counter
      lr = !lr;
      lr_counter = 0;
    }
    else
    {
      // No toggle yet, keep counting
      lr_counter += 1;
    }  
  }
  
  // Resets
  if(!reset_n)
  {
    overflow = 0;
    sclk_counter = 0;
    sclk = 0;
    lr_counter = 0;
    lr = 0;
  }
  
  // Drive the outgoing I2S signals
  write_i2s_pmod(to_i2s);
}