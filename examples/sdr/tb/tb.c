// Test bench
#include "intN_t.h"
#include "uintN_t.h"
#include "arrays.h"
#include "stream/stream.h"
#pragma PART "xc7a100tcsg324-1"

// Functions to test
//#include "../fm_radio.h"

DECL_STREAM_TYPE(int16_t)
DECL_STREAM_TYPE(int40_t)

// TEST SLOW FIR INTERP

#define slow_fir_interp_name slow_interp_24x
#define SLOW_FIR_INTERP_N_TAPS 227
#define SLOW_FIR_INTERP_II_IN 500 // After 500x front end decim
#define SLOW_FIR_INTERP_FACTOR 24
#define SLOW_FIR_INTERP_II_OUT FLOOR_DIV(SLOW_FIR_INTERP_II_IN,SLOW_FIR_INTERP_FACTOR)
#define slow_fir_interp_data_t int16_t
#define slow_fir_interp_coeff_t int16_t
#define slow_fir_interp_accum_t int40_t // data_width + coeff_width + log2(taps#)
#define slow_fir_interp_out_t int16_t
#define SLOW_FIR_INTERP_OUT_SCALE 3 // normalize, // 3x then 8x(<<3) w pow2 scale = 24
#define SLOW_FIR_INTERP_POW2_DN_SCALE (15-3) // data_width + coeff_width - out_width - 1 // fixed point adjust
// Declare the binary tree adder used in this slow FIR
#include "slow_binary_tree.h"
// Termination base case of tree
// II=1 tree of 2 elements
// do_not_use_slow_funcs_with_II_of_1
stream(slow_fir_interp_accum_t) term(slow_fir_interp_accum_t data[2], uint1_t valid){
  stream(slow_fir_interp_accum_t) rv;
  rv.data = data[0] + data[1];
  rv.valid = valid;
  return rv;
}
DECL_SLOW_BINARY_TREE(
  tree16, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,16), term,
  slow_fir_interp_accum_t, slow_fir_interp_accum_t, slow_fir_interp_accum_t, +, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,8)
)
DECL_SLOW_BINARY_TREE(
  tree8, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,8), tree16,
  slow_fir_interp_accum_t, slow_fir_interp_accum_t, slow_fir_interp_accum_t, +, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,4)
)
DECL_SLOW_BINARY_TREE(
  tree4, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,4), tree8,
  slow_fir_interp_accum_t, slow_fir_interp_accum_t, slow_fir_interp_accum_t, +, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,2)
)
DECL_SLOW_BINARY_TREE(
  tree2, CEIL_DIV(SLOW_FIR_INTERP_II_OUT,2), tree4,
  slow_fir_interp_accum_t, slow_fir_interp_accum_t, slow_fir_interp_accum_t, +, SLOW_FIR_INTERP_II_OUT // div by 1
)
// ^ Component trees decreasing II and number elements(=II to start) by factor elements 2
// Base binary tree
#define slow_fir_interp_binary_tree_func PPCAT(slow_fir_interp_name,_binary_adder_tree)
DECL_SLOW_BINARY_TREE(
  slow_fir_interp_binary_tree_func, SLOW_FIR_INTERP_II_OUT, tree2,
  slow_fir_interp_accum_t, slow_fir_interp_accum_t, slow_fir_interp_accum_t, +, SLOW_FIR_INTERP_N_TAPS
)
#define SLOW_FIR_INTERP_COEFFS { \
  -173, \
  -34,  \
  -36,  \
  -39,  \
  -41,  \
  -44,  \
  -45,  \
  -47,  \
  -48,  \
  -49,  \
  -49,  \
  -49,  \
  -48,  \
  -46,  \
  -44,  \
  -42,  \
  -38,  \
  -35,  \
  -30,  \
  -25,  \
  -20,  \
  -13,  \
  -7,   \
  0,    \
  7,    \
  15,   \
  23,   \
  31,   \
  39,   \
  48,   \
  56,   \
  64,   \
  71,   \
  78,   \
  85,   \
  91,   \
  97,   \
  101,  \
  105,  \
  108,  \
  110,  \
  110,  \
  110,  \
  108,  \
  105,  \
  100,  \
  95,   \
  88,   \
  79,   \
  70,   \
  59,   \
  47,   \
  34,   \
  20,   \
  6,    \
  -10,  \
  -26,  \
  -42,  \
  -59,  \
  -75,  \
  -92,  \
  -108, \
  -124, \
  -139, \
  -153, \
  -166, \
  -177, \
  -187, \
  -195, \
  -201, \
  -205, \
  -207, \
  -206, \
  -203, \
  -197, \
  -188, \
  -176, \
  -161, \
  -143, \
  -122, \
  -97,  \
  -70,  \
  -40,  \
  -8,   \
  28,   \
  66,   \
  107,  \
  150,  \
  194,  \
  241,  \
  289,  \
  338,  \
  389,  \
  440,  \
  491,  \
  542,  \
  593,  \
  643,  \
  693,  \
  741,  \
  787,  \
  831,  \
  873,  \
  913,  \
  949,  \
  983,  \
  1013, \
  1040, \
  1063, \
  1082, \
  1096, \
  1107, \
  1114, \
  1116, \
  1114, \
  1107, \
  1096, \
  1082, \
  1063, \
  1040, \
  1013, \
  983,  \
  949,  \
  913,  \
  873,  \
  831,  \
  787,  \
  741,  \
  693,  \
  643,  \
  593,  \
  542,  \
  491,  \
  440,  \
  389,  \
  338,  \
  289,  \
  241,  \
  194,  \
  150,  \
  107,  \
  66,   \
  28,   \
  -8,   \
  -40,  \
  -70,  \
  -97,  \
  -122, \
  -143, \
  -161, \
  -176, \
  -188, \
  -197, \
  -203, \
  -206, \
  -207, \
  -205, \
  -201, \
  -195, \
  -187, \
  -177, \
  -166, \
  -153, \
  -139, \
  -124, \
  -108, \
  -92,  \
  -75,  \
  -59,  \
  -42,  \
  -26,  \
  -10,  \
  6,    \
  20,   \
  34,   \
  47,   \
  59,   \
  70,   \
  79,   \
  88,   \
  95,   \
  100,  \
  105,  \
  108,  \
  110,  \
  110,  \
  110,  \
  108,  \
  105,  \
  101,  \
  97,   \
  91,   \
  85,   \
  78,   \
  71,   \
  64,   \
  56,   \
  48,   \
  39,   \
  31,   \
  23,   \
  15,   \
  7,    \
  0,    \
  -7,   \
  -13,  \
  -20,  \
  -25,  \
  -30,  \
  -35,  \
  -38,  \
  -42,  \
  -44,  \
  -46,  \
  -48,  \
  -49,  \
  -49,  \
  -49,  \
  -48,  \
  -47,  \
  -45,  \
  -44,  \
  -41,  \
  -39,  \
  -36,  \
  -34,  \
  -173  \
}
#include "dsp/slow_fir_interp.h"


// IQ samples generated by python script
// Script not setup to generate+check other than i16
#define in_data_t int16_t
#define out_data_format "%d"
#include "samples.h"

// Configure IO for the unit under test
#define in_stream_t stream(int16_t)
#define out_stream_t stream(int16_t)
#define DO_IQ_IN_IQ_OUT \
i_output = slow_interp_24x(i_input); \
q_output.data = 0; \
q_output.valid = 1;

// The test bench sending samples into a module and printing output sampples
#pragma MAIN tb
void tb()
{
  static uint32_t cycle_counter;
  static in_data_t i_samples[I_SAMPLES_SIZE] = I_SAMPLES;
  static in_data_t q_samples[Q_SAMPLES_SIZE] = Q_SAMPLES;
  static uint1_t samples_valid[SAMPLES_VALID_SIZE] = SAMPLES_VALID;

  // Prepare input sample into DUT
  in_stream_t i_input;
  i_input.data = i_samples[0];
  i_input.valid = samples_valid[0];
  in_stream_t q_input;
  q_input.data = q_samples[0];
  q_input.valid = samples_valid[0];

  // Do one clock cycle, input valid and get output
  out_stream_t i_output;
  out_stream_t q_output;
  DO_IQ_IN_IQ_OUT

  // Print valid output samples
  if(i_output.valid&q_output.valid){
    printf("Cycle,%d,Sample IQ =,"out_data_format","out_data_format"\n", cycle_counter, i_output.data, q_output.data);
  }

  // Prepare for next sample (rotating to loop samples if run longer)
  ARRAY_1ROT_DOWN(in_data_t, i_samples, I_SAMPLES_SIZE)
  ARRAY_1ROT_DOWN(in_data_t, q_samples, Q_SAMPLES_SIZE)
  ARRAY_1ROT_DOWN(uint1_t, samples_valid, SAMPLES_VALID_SIZE)
  cycle_counter+=1;
}