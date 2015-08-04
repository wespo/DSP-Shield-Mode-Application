#ifndef _IIRCODE_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _IIRCODE_H_INCLUDED

#include "core.h"
#include "SD.h"
#include "Audio_exposed.h"
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

#define TYPE_BUTTER 0
#define TYPE_BESSEL 1
#define TYPE_ELLIP 2
#define TYPE_CHEBY 3

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

iirConfig initIIR(); //initialize channel without configuring filter buffers
iirConfig initIIR(long* buffer, int *coeff); //initialize channel

int loadfilterIIR(char *fresponse, char *fpass, int Hz, int *target, int order); //loads an IIR filter into a buffer (target) from disk.
void recvfilterIIR(iirConfig &config, int order, int *coeffs); //loads an IIR filter into a buffer (config) from disk.

void IIRsumChannels(iirConfig &one, iirConfig &two, int len); //sums the output buffers of two iirConfigs, divided by two to prevent overflos
void IIRProcessChannel(iirChannel &channel); //process both iir filters in the channel.
iirChannel newIIRChannel(long* bufferL, long* bufferH, int* coeffL, int* coeffH); //initailize the channel.
void configureIIRChannel(iirChannel &channel, int mode, int* bufferin, int* bufferout, int* bufferint); //configure the channel's input and output buffers.
void deconfigureIIRChannel(iirChannel &channel); //disable channel, point everything to zero (watch out).

void processIIR(iirConfig &config); //process an IIR channel

void IIRRecieve(int channel); //recieve new coefficients for LPF or HPF. channel == left or right (0, 1);
void IIRRecieveDual(int channel); //recieve new coefficients for BPF or BSF. channel == left or right (0, 1);

void printIIRData(iirChannel &channel); //debug print. Prints coefficients.
void printFilterData(iirConfig &filter); //
void IIRLoad(int command, int channel);
extern iirChannel iirL, iirR; //two IIR Channels for stereo.


//external buffers.
extern int IIRcoeffsL_L[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferL_L[IIR_DELAY_BUF_SIZE];

extern int IIRcoeffsR_L[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferR_L[IIR_DELAY_BUF_SIZE];

extern int IIRcoeffsL_H[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferL_H[IIR_DELAY_BUF_SIZE];

extern int IIRcoeffsR_H[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2];
extern long IIRdelayBufferR_H[IIR_DELAY_BUF_SIZE];

#endif
