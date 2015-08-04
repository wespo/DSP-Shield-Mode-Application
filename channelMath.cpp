#include "channelMath.h"
#include <DSPLIB.h>
#include "Audio_exposed.h"

void mathChannelInit(int* inL, int* outL, int* inR, int* outR, mathChannelConfig &config)
{
  config.inL = inL;
  config.outL = outL;
  config.modeL = MATH_NONE; //initialize to 0
  config.inR = inR;
  config.outR = outR;
  config.modeR = MATH_NONE; //initialize to 0
}

void mathChannelMode(mathChannelConfig &config, int channel, int mode)
{
  if(channel == 0)
  {
    config.modeL = mode;
  }
  else if(channel == 1)
  {
    config.modeR = mode;
  }
  else if(channel == 2)
  {
    config.modeL = mode;
    config.modeR = mode;
  }
}

void processMathChannels(mathChannelConfig &config)
{
  if((config.modeL != MATH_NONE) && (config.modeR == config.modeL)) //both nonzero and the same
  {
    processMathChannel(config.inL, config.inR, config.outL, config.modeL);
    memcpy(config.outR, config.outL, I2S_DMA_BUF_LEN); //normal, just copy one block in.
  }
  else //evaluate each one separately.
  {
    if(config.modeL != MATH_NONE)
    {
      processMathChannel(config.inL, config.inR, config.outL, config.modeL);
    }
    if(config.modeR != MATH_NONE)
    {
      processMathChannel(config.inR, config.inL, config.outR, config.modeR);
    }
  }
}

void processMathChannel(int* src1, int* src2, int* dst, int mode)
{
  if(mode == MATH_SUB)
  {
    sub((DATA *) src1, (DATA *) src2, (DATA *) dst, I2S_DMA_BUF_LEN, 0); 
  }
  else if(mode == MATH_ADD)
  {
    add((DATA *) src1, (DATA *) src2, (DATA *) dst, I2S_DMA_BUF_LEN, 0); 
  }
}
