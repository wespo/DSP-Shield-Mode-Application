#ifndef _REVERB_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _REVERB_H_INCLUDED

#include "Audio.h"

#define MAX_DELAY 7200



class reverbClass {
  public:
    reverbClass();
    reverbClass(int delaySamps, int* in, int* out);
    void init(int delaySamps, int* in, int* out);
    void setReverbDelay(int delaySamps);
    void setInputBuffer(int* in);
    void setOutputBuffer(int* out);
    void processReverb();
  
  private:
    unsigned long reverbPointer;
    //int* reverbBuffer;
    int reverbBuffer[MAX_DELAY + I2S_DMA_BUF_LEN];
    int* input;
    int* output;
    unsigned long reverbLength;
  
};

#endif
