#include "reverb.h"
#include <DSPLIB.h>
#include "Audio.h"  

reverbClass::reverbClass()
{
  init(0,0,0);
}
reverbClass::reverbClass(int delaySamps, int* in, int* out)
{
  init(delaySamps, in, out);
}

void reverbClass::init(int delaySamps, int* in, int* out)
{
  setInputBuffer(in);
  setOutputBuffer(out);
  setReverbDelay(delaySamps);
}

void reverbClass::setReverbDelay(int delaySamps)
{
  if(delaySamps > MAX_DELAY) //protect against overrunning the buffer
  {
    delaySamps = MAX_DELAY;
  }
//  free(reverbBuffer);
//  reverbBuffer = NULL;
//  if(delaySamps > 0)
//  {
//    reverbBuffer = (int*)malloc(delaySamps + I2S_DMA_BUF_LEN);
//  }
  reverbLength = delaySamps;  
  reverbPointer = 0;
}
void reverbClass::setInputBuffer(int* in)
{
  input = in;
}
void reverbClass::setOutputBuffer(int* out)
{
  output = out;
}
void reverbClass::processReverb()
{
  if(reverbLength > 0)
  {
    for(int i = 0; i < I2S_DMA_BUF_LEN; i++) //copy new data in
    {
      int index = (reverbPointer + reverbLength + i) % (reverbLength + I2S_DMA_BUF_LEN); //circular index to buffer
      reverbBuffer[index] = input[i];
    }
    
    for(int i = 0; i < I2S_DMA_BUF_LEN; i++) //add arrays
    {
      int index = (reverbPointer + i) % (reverbLength + I2S_DMA_BUF_LEN); //circular index to buffer
      output[i] = input[i] + reverbBuffer[index];
    }
    reverbPointer = (reverbPointer + I2S_DMA_BUF_LEN) % (reverbLength + I2S_DMA_BUF_LEN);
  }
}
