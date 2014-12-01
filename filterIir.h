#ifndef _IIRCODE_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _IIRCODE_H_INCLUDED

#include "ddsCode.h"
#include "filter.h"

//IIR Buffers
#define IIR_ORDER_MAX    (40)
#define DELAY_COUNT      (5)
#define COEFFS_PER_BIQUAD (7)
#define IIR_DELAY_BUF_SIZE   ((IIR_ORDER_MAX/2)*(DELAY_COUNT))

//each struct contains two filter blocks, and 
struct iirConfig {
  int* src;
  int* dst;
  int* coeffs;
  int* delayBuf;
  int  order;
  int  enabled;
} lpfL, lpfR, hpfL, hpfR;



volatile int IIRTagL = 0;
int IIRcoeffsL[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
int IIROrderL = 0;
long IIRdelayBufferL[IIR_DELAY_BUF_SIZE] = {0};

volatile int IIRTagR = 0;
int IIRcoeffsR[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
int IIROrderR = 0;
long IIRdelayBufferR[IIR_DELAY_BUF_SIZE] = {0};

iirConfig initIIR();
iirConfig initIIR(int *src, int *dst);
iirConfig configIIR(int *src, int *dst);

void loadfilterIIR(char *ftype, char *fresponse, char *fpass, int Hz, int *target, int order);
void recvfilterIIR(iirConfig &config, int order, int *coeffs);
void muxIIR(iirConfig &config, int *src, int *dst);

void processIIR(iirConfig &config);

#endif
