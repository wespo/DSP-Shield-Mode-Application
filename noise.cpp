#include "noise.h"


noiseConfig noiseConfigLeft, noiseConfigRight;
void noiseGen(noiseConfig &config, int buffer[], int frameSize)
{
  if(config.enable == 1)
  {
    if(config.muxMode == MUX_OVERWRITE) //overwrite mode
    {
    	rand16((DATA *)buffer, frameSize);
    	gain(config.gain, buffer, frameSize);
    }
    else if(config.muxMode == MUX_SUM)
    {
        int* tempRand = new int[frameSize]; 
    	rand16((DATA *)tempRand, frameSize);
    	gain(config.gain, tempRand, frameSize);
    	add((DATA *) tempRand, (DATA *) buffer, (DATA *) buffer, frameSize, 0);
        delete tempRand;
    }
  }
}

void gain(int gain, int buffer[], int frameSize)
{
	for(int i = 0; i < frameSize; i++)
	{
		long a = buffer[i]>>1;
        long b = gain;
        long c = ((long)(a * b)) >> 15;
        buffer[i] = (int) c;
	}
}

void noiseConfigInit(noiseConfig &config)
{
  config.enable = 0;
  config.gain = 0.3;
  config.muxMode = 0;
}

noiseConfig noiseInit(float gain, int sumMode)
{
    noiseConfig newConfig; //structure for the new configuration
    noiseConfigInit(newConfig);
    //populate the new configuration
    newConfig.muxMode = sumMode;
    newConfig.gain = int(gain*32767);
    newConfig.enable = 1;
    rand16init();
    return newConfig;
}

void noiseStart(int channel)
{
  //parse out two floats, frequency and gain from mailbox.
   float gain;
   long recon = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
   recon <<= 16;
   recon += (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
   memcpy(&gain, &recon, sizeof(gain));
   int sumMode = (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8];
   noiseConfig newConfig = noiseInit(gain, sumMode); //ddsInit builds a configuration

   if(channel == CHAN_LEFT)
   {
    noiseConfigLeft = newConfig;
   }
   else if(channel == CHAN_RIGHT)
   {
    noiseConfigRight = newConfig;
   }
   else if(channel == CHAN_BOTH)
   {
    noiseConfigLeft = newConfig;
    noiseConfigRight = newConfig;
   }
}

void noiseStop(int channel)
{
   if(channel == CHAN_LEFT)
   {
     noiseConfigLeft.enable = 0;
   }
   else if(channel == CHAN_RIGHT)
   {
     noiseConfigRight.enable = 0;
   }
   else if(channel == CHAN_BOTH)
   {
     noiseConfigLeft.enable = 0;
     noiseConfigRight.enable = 0;
   }
}
