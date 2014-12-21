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
#define IIR_DELAY_BUF_SIZE   (200)//((IIR_ORDER_MAX/2)*(DELAY_COUNT))

//general defines
#define CHAN_LEFT 0
#define CHAN_RIGHT 1
#define CHAN_BOTH 2

#define ALL_PASS 1
#define LOW_PASS 1
#define HIGH_PASS 2
#define BAND_PASS 3
#define BAND_STOP 4

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
iirConfig initIIR(long* buffer, int *coeff);

void loadfilterIIR(char *ftype, char *fresponse, char *fpass, int Hz, int *target, int order); //loads an IIR filter into a buffer (target) from disk.
void recvfilterIIR(iirConfig &config, int order, int *coeffs); //loads an IIR filter into a buffer (config) from disk.

void IIRsumChannels(iirConfig &one, iirConfig &two, int len); //sums the output buffers of two iirConfigs, divided by two to prevent overflos
void IIRProcessChannel(iirChannel &channel);
iirChannel newIIRChannel(long* bufferL, long* bufferH, int* coeffL, int* coeffH);
void configureIIRChannel(iirChannel &channel, int mode, int* bufferin, int* bufferout, int* bufferint);

void processIIR(iirConfig &config);

void printIIRData(iirChannel &channel);
void printFilterData(iirConfig &filter);
extern iirChannel iirL, iirR;


//extern int* IIRcoeffsL_L, IIRcoeffsR_L, IIRcoeffsL_H, IIRcoeffsR_H;
//extern long* IIRdelayBufferL_L, IIRdelayBufferR_L, IIRdelayBufferL_H, IIRdelayBufferR_H;
extern int IIRcoeffsL_L[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferL_L[IIR_DELAY_BUF_SIZE];

extern int IIRcoeffsR_L[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferR_L[IIR_DELAY_BUF_SIZE];

extern int IIRcoeffsL_H[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferL_H[IIR_DELAY_BUF_SIZE];

extern int IIRcoeffsR_H[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferR_H[IIR_DELAY_BUF_SIZE];
#endif
