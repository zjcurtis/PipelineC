// gcc fft_demo.c -lm -o fft_demo && ./fft_demo
#include "fft_demo.h"

#define NFFT (1<<7) // 128
// Set sample rate for working out units
#define SAMPLE_RATE_INT_HZ 44100 // Fs in integer Hz
#define TONE_RATE_INT_HZ 10000 // integer Hz
//#define TONE_IS_COMPLEX

// For demo writing frame buffer is putting pixel in std out for fft_demo.py to plot
void frame_buf_write(uint16_t x, uint16_t y, uint8_t color)
{
    printf("x,y,c,%d,%d,%d\n", x, y, color);
}

void color_screen(int width, int height, float* pwr_bins, uint32_t n_bins){
  // TODO use output power to draw on screen
  // For now all white...
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      frame_buf_write(i, j, 255);
    }
  }
}

int main(){

    // Create arrays 
    fft_in_t* input = (fft_in_t*)malloc(NFFT * sizeof(fft_in_t));
    fft_out_t* output = (fft_out_t*)malloc(NFFT * sizeof(fft_out_t));
    float* output_pwr = (float*)malloc(NFFT * sizeof(float));

    // Gen Test signal
    printf("fs,%d\n", SAMPLE_RATE_INT_HZ);
    #ifdef FFT_TYPE_IS_FIXED
    uint32_t sec_per_sample =  (INT16_MAX+1) / SAMPLE_RATE_INT_HZ; // 1/fs seconds
    #endif
    #ifdef FFT_TYPE_IS_FLOAT
    float sec_per_sample = 1.0 / SAMPLE_RATE_INT_HZ; // 1/fs seconds
    #endif
    for (uint32_t i = 0; i < NFFT; i++)
    {
        input[i] = exp_complex(TONE_RATE_INT_HZ*(i*sec_per_sample));
        #ifndef TONE_IS_COMPLEX
        input[i].imag = 0;
        #endif
    }

    // print input as float 
    for (uint32_t i = 0; i < NFFT; i++)
    {
      #ifdef FFT_TYPE_IS_FIXED
      float xi = (float)input[i].real / (float)INT16_MAX;
      float xj = (float)input[i].imag / (float)INT16_MAX;
      printf("i,xi,xj,%d,%f,%f\n", i, xi, xj);
      #endif
      #ifdef FFT_TYPE_IS_FLOAT
      printf("i,xi,xj,%d,%f,%f\n", i, input[i].real, input[i].imag);
      #endif
    }

    // apply fft
    compute_fft_cc(input, output, NFFT);

    // print results 
    for (uint32_t i = 0; i < NFFT; i++)
    {
        // Shift is handled in fft_demo.py, not here
        //uint32_t j = i < (NFFT>>1) ? (NFFT>>1)+i : i-(NFFT>>1); // FFT SHIFT
        //int fj = i-(NFFT>>1);
        //float re_j = (float)output[j].real / (float)INT16_MAX;
        //float im_j = (float)output[j].imag / (float)INT16_MAX;
        #ifdef FFT_TYPE_IS_FIXED
        float re = (float)output[i].real / (float)INT16_MAX;
        float im = (float)output[i].imag / (float)INT16_MAX;
        #endif
        #ifdef FFT_TYPE_IS_FLOAT
        float re = output[i].real;
        float im = output[i].imag;
        #endif
        float pwr2 = (re*re) + (im*im);
        float pwr = pwr2 > 0 ? sqrtf(pwr2) : 0;
        printf("i,re,im,p,%d,%f,%f,%f\n", i, re, im, pwr);
    }

    // Print screen coloring results (640x480 ratio image)
    // Only use positive freq power in first half of array
    color_screen(64, 48, output_pwr, NFFT/2);
}