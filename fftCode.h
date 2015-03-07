//FFT Handling Code.

#ifndef _FFTCODE_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _FFTCODE_H_INCLUDED

#include "core.h"
#include "SD.h"
#include "Audio.h"
#include "mailbox.h"
#include <math.h>
#include "filter.h"


//FFT Buffer
#define WINDOW_LENGTH 1024 //(I2S_DMA_BUF_LEN) //4096

#define SOURCE_INPUT 0
#define SOURCE_OUTPUT 1
#define SOURCE_CODEC 2

#define WINDOW_RECT 0
#define WINDOW_BLACKMAN 1
#define WINDOW_HAMM 2
#define WINDOW_HANN 3
#define WINDOW_NONE 4

#define SPECTRUM_COMPLEX 0
#define SPECTRUM_MAGNITUDE 1

//general defines
#define CHAN_LEFT 0
#define CHAN_RIGHT 1
#define CHAN_BOTH 2

struct fftConfig { //spectrum variables
	int enable;
	int count;
	int bufferReady;
	int updateRate;
	int source;
	int channel;
	int length;
        int complex;
        int windowType;
        int *window;
        int* buffer;
};



void windowSignal(int * source, int * destination, int *window, int length); //windows a signal.
fftConfig FFTInit(); //returns a new FFTConfig
fftConfig configFFT(int enable, int updateRate, int source, int channel, int complex, int windowType, int length); //configures an FFTConfig
void updateSpectrumPointer(fftConfig &config, int* in, int* out, int ** codec); //if the current mode is SOURCE_CODEC
void spectrum(fftConfig &config); //takes a signal and returns the power spectrum;
void sendSpectrum(fftConfig &config); //sends the current spectrum for the given configuration
void loadWindow(fftConfig &config);
void initializeFFTSpectrum(int channel);
void disableFFTSpectrum(int channel);

extern fftConfig fftConfigLeft, fftConfigRight;
#endif
