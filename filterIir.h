#ifndef _IIRCODE_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _IIRCODE_H_INCLUDED

#include "core.h"
#include "SD.h"
#include "Audio.h"
#include "filter.h"

//IIR Buffers
#define IIR_ORDER_MAX    (40)
#define DELAY_COUNT      (5)
#define COEFFS_PER_BIQUAD (7)
#define IIR_DELAY_BUF_SIZE   ((IIR_ORDER_MAX/2)*(DELAY_COUNT))

#define NONE 0 //no filtering
#define LPF 1 //lowpass
#define HPF 2 //highpass
#define BPF 3 //bandpass
#define BSF 4 //bandstop

#define LEFT 0
#define RIGHT 1

//each struct contains two filter blocks, and 
struct iirConfig {
  int* src;
  int* dst;
  int* coeffs;
  int  order;
  long* delayBuf;
  int  enabled;
};

struct iirChannel {
  iirConfig hpf;
  iirConfig lpf;
  int mode; //lpf, hpf, bpf, bsf 
};

iirConfig initIIR();
iirConfig initIIR(int *src, int *dst);
iirConfig configIIR(int *src, int *dst);

void loadfilterIIR(char *ftype, char *fresponse, char *fpass, int Hz, int *target, int order); //loads an IIR filter into a buffer (target) from disk.
void recvfilterIIR(iirConfig &config, int order, int *coeffs); //loads an IIR filter into a buffer (config) from disk.

void IIRsumChannels(iirConfig &one, iirConfig &two, int len); //sums the output buffers of two iirConfigs, divided by two to prevent overflos
void IIRProcessChannel(iirChannel &channel);
iirChannel newIIRChannel();
void configureIIRChannel(iirChannel &channel, int mode, int* bufferin, int* bufferout, int* bufferint);

void processIIR(iirConfig &config);

extern iirChannel iirL, iirR;

#endif
