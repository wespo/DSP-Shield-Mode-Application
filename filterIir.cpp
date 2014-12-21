#include "filterIir.h"
#include "mailbox.h"

#define FILTER_LENGTH_DEFAULT (201)
#define FILTER_LENGTH_MAX (511)
#define FILE_CHUNK (100)

// input samples
extern int filterIn1[I2S_DMA_BUF_LEN];
extern int filterIn2[I2S_DMA_BUF_LEN];

// intermeidate processing samples
extern int filterInt1[I2S_DMA_BUF_LEN];
extern int filterInt2[I2S_DMA_BUF_LEN];

// output samples
extern int filterOut1[I2S_DMA_BUF_LEN];
extern int filterOut2[I2S_DMA_BUF_LEN];

inline void setXF(bool togg)
{
  if(togg)
    asm(" BIT(ST1, #13) = #1"); //flag that we had an incident and try to recover from it.
  else
    asm(" BIT(ST1, #13) = #0"); //flag that we had an incident and try to recover from it.

}
//iirConfig lpfL, lpfR, hpfL, hpfR;
iirChannel iirL, iirR;

int IIRcoeffsL_L[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
long IIRdelayBufferL_L[IIR_DELAY_BUF_SIZE] = {0};

int IIRcoeffsR_L[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
long IIRdelayBufferR_L[IIR_DELAY_BUF_SIZE] = {0};

int IIRcoeffsL_H[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
long IIRdelayBufferL_H[IIR_DELAY_BUF_SIZE] = {0};

int IIRcoeffsR_H[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
long IIRdelayBufferR_H[IIR_DELAY_BUF_SIZE] = {0};

void loadfilterIIR(char* ftype, char *fresponse, char *fpass, int Hz, int* target, int order)
{
  File          fileHandle;
  //int newCoeffs[FILTER_LENGTH_MAX] = {0};

  long shortHz = Hz;
  shortHz -= 10; //lowest cutoff frequency = 10Hz
  int fileBin = shortHz / FILE_CHUNK; //alias to 1000Hz files.
  shortHz %= FILE_CHUNK; //alias to 1000Hz bins.
  shortHz *= order; //201 tap long filters;
  shortHz *= 2; // 2 bytes per tap;
  char fileName[32];
  sprintf(fileName, "%s/%d/%s/%d.flt",ftype,fresponse,order,fpass,fileBin); //default file name

#ifdef AUDIO_INTERRUPTION
  AudioC.detachIntr(); //turning off the audio fixes audio / sd card collision
#endif
  fileHandle = SD.open(fileName, FILE_READ);
  if(fileHandle)
  {

    fileHandle.seek(shortHz);
    fileHandle.read(target, order); //load the data
    fileHandle.close();

    for(int i = 0; i < order; i++) //fix endian-ness of dataset.
    {
      int temp = target[i];
      target[i] = ((temp & 0x00FF)<<8) + ((temp & 0xFF00)>>8);
    }          
  }

#ifdef AUDIO_INTERRUPTION
  bool status = AudioC.Audio(TRUE);
  AudioC.setSamplingRate(SAMPLING_RATE_44_KHZ);
  if (status == 0)
  {
    AudioC.attachIntr(dmaIsr);
  }
#endif
}

iirConfig initIIR(long* buffer, int *coeff)
{
  iirConfig config;
  config.src = 0;
  config.dst = 0;
  config.coeffs = coeff;
  config.delayBuf = buffer;
  config.order = 0;
  config.enabled = 0;
  return config;

}
iirConfig initIIR()
{
  return(initIIR(0,0)); //don't initialize coeffs and delay buffer.
}

inline void processIIR(iirConfig &config)
{
  if(config.enabled)
  {
    if(config.src == config.dst) //process in place if possible to save cycles.
    {
      filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, config.src, config.coeffs, config.delayBuf, config.order);  //filter in place src = dst
    }
    else
    {
      filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, config.src, config.dst, config.coeffs, config.delayBuf, config.order);  //filter in place src = dst
    }
  }

}
iirChannel newIIRChannel(long* bufferL, long* bufferH, int* coeffL, int* coeffH) //initialize IIR channel
{
  iirChannel newChannel;
  //initialize 4 IIR blocks to point to the inputs.
  newChannel.lpf = initIIR(bufferL, coeffL);
  newChannel.hpf = initIIR(bufferH, coeffH);
  newChannel.mode = 0;
  return newChannel;
}

void configureIIRChannel(iirChannel &channel, int mode, int* bufferin, int* bufferout, int* bufferint)
//the HIGH_PASS destination buffer should always contain the final result of all filter operations.
{
  channel.mode = mode;
  if(mode == LOW_PASS)
  {
    //low pass filter, configured such that the lpf directly goes 
    //in->[lpf]->out
    channel.lpf.src = bufferin;
    channel.lpf.dst = bufferin;

    channel.hpf.src = bufferin;
    channel.hpf.dst = bufferin;

    channel.lpf.enabled = 1;
    channel.hpf.enabled = 0;
  }
  else if(mode == HIGH_PASS)
  {
    //high pass filter, configured such that the hpf directly goes 
    //in->[hpf]->out
    channel.lpf.src = bufferin;
    channel.lpf.dst = bufferin;

    channel.hpf.src = bufferin;
    channel.hpf.dst = bufferin;

    channel.lpf.enabled = 0;
    channel.hpf.enabled = 1;    
  }
  else if(mode == BAND_PASS)
  {
    //band pass filter, configured such that the filters are cascaded. 
    //in->[lpf]->intermediate->[hpf]->out
    channel.lpf.src = bufferin;
    channel.lpf.dst = bufferin;

    channel.hpf.src = bufferin;
    channel.hpf.dst = bufferin;

    channel.lpf.enabled = 1;
    channel.hpf.enabled = 1;
  }
  else if(mode == BAND_STOP)
  {
    //band stop filter, configured such that the filters are paralleled. 
    //in->[lpf]->intermediate, in->[hpf]->out, (out + intermediate) / 2 -> out 
    //(handled by the IIRProcessChannel function)
    channel.lpf.src = bufferin;
    channel.lpf.dst = bufferint;

    channel.hpf.src = bufferin;
    channel.hpf.dst = bufferout;

    channel.lpf.enabled = 1;
    channel.hpf.enabled = 1;
  }
  else //no filters enabled, point the output to the input, so that the fir filter routine copies the data out.
  {
    channel.lpf.src = bufferin;
    channel.lpf.dst = bufferin;

    channel.hpf.src = bufferin;
    channel.hpf.dst = bufferin;

    channel.lpf.enabled = 0;
    channel.hpf.enabled = 0;

  }
}

void deconfigureIIRChannel(iirChannel &channel)
{
  channel.lpf = initIIR(0,0);
  channel.hpf = initIIR(0,0);
}

void IIRProcessChannel(iirChannel &channel)
{
  if (channel.mode == LOW_PASS) //low pass
  {
    processIIR(channel.lpf);
  }
  else if (channel.mode == HIGH_PASS) //high pass
  {
    processIIR(channel.hpf);
  }
  else if (channel.mode == BAND_PASS) //band pass
  {
    processIIR(channel.lpf);
    processIIR(channel.hpf);    
  }
  else if(channel.mode == BAND_STOP) //band stop
  {
    processIIR(channel.lpf);
    processIIR(channel.hpf);
    IIRsumChannels(channel.hpf, channel.lpf, I2S_DMA_BUF_LEN);
  }

}

inline void IIRsumChannels(iirConfig &one, iirConfig &two, int len) //sums the output buffers of two iirConfigs, divided by two to prevent overflows. Stores the result in one.
{
  for(int i = 0; i < len; i++)
  {
    one.dst[i] = (one.dst[i] >> 1) + (two.dst[i] >> 1); //prevents overflow.
  }
}
void printFilterData(iirConfig &filter)
{
  Serial.println(filter.order);
  for(int i = 0; i < filter.order/2 * 7; i++)
  {
    Serial.print(filter.coeffs[i]);
    Serial.print(", ");
    if(i%7 == 6)
    {
      Serial.print('\n');
    }
  }
}
void printIIRData(iirChannel &channel)
{
  Serial.begin(115200);
  printFilterData(channel.lpf);
  printFilterData(channel.hpf);     
  Serial.end(); 
}

void IIRRecieve(int channel)
{
     int order = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
     int dest = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
     int* newData = (int*) malloc(shieldMailbox.inboxSize/2);  
     //Working read via serial code
     for(int i = 8; i < shieldMailbox.inboxSize; i = i + 2) //copy recieved coefficients to buffer.
     {
       newData[i/2 - 4] = ((shieldMailbox.inbox[i+1]<<8) + shieldMailbox.inbox[i]);
     }
     
     if(dest == LOW_PASS)
     {
       int newDataLen = shieldMailbox.inboxSize/2 - 4;
       //int newDataLen = order/2*COEFFS_PER_BIQUAD;
       if(channel == CHAN_LEFT) //channel 0 == left
       {
         iirL.lpf.order = order;
         memcpy(iirL.lpf.coeffs, newData, newDataLen); //copy into low pass coefficients
         configureIIRChannel(iirL,LOW_PASS,filterIn1, filterOut1, filterInt1);  //configure blocks
       }
       else if (channel == CHAN_RIGHT) //channel 1 == right
       {
         iirR.lpf.order = order;
         memcpy(iirR.lpf.coeffs, newData, newDataLen); //copy into low pass coefficients
         configureIIRChannel(iirR,LOW_PASS,filterIn2, filterOut2, filterInt2); //configure blocks
       }
       else if (channel == CHAN_BOTH) //channel 2 == both
       {
         iirL.lpf.order = order;
         memcpy(iirL.lpf.coeffs, newData, newDataLen); //copy into low pass coefficients
         
         iirR.lpf.order = order;
         memcpy(iirR.lpf.coeffs, newData, newDataLen); //copy into low pass coefficients

         configureIIRChannel(iirL,LOW_PASS,filterIn1, filterOut1, filterInt1); //configure blocks
         configureIIRChannel(iirR,LOW_PASS,filterIn2, filterOut2, filterInt2);
       }
       
     }
     else if(dest == HIGH_PASS)
     {
       int newDataLen = shieldMailbox.inboxSize/2 - 4;
       //int newDataLen = order/2*COEFFS_PER_BIQUAD;
       if(channel == CHAN_LEFT) //channel 0 == left
       {
         iirL.hpf.order = order;
         memcpy(iirL.hpf.coeffs, newData, newDataLen); //copy into low pass coefficients
         configureIIRChannel(iirL,HIGH_PASS,filterIn1, filterOut1, filterInt1);  //configure blocks
       }
       else if (channel == CHAN_RIGHT) //channel 1 == right
       {
         iirR.hpf.order = order;
         memcpy(iirR.hpf.coeffs, newData, newDataLen); //copy into low pass coefficients
         configureIIRChannel(iirR,HIGH_PASS,filterIn2, filterOut2, filterInt2); //configure blocks
       }
       else if (channel == CHAN_BOTH) //channel 2 == both
       {
         iirL.hpf.order = order;
         memcpy(iirL.hpf.coeffs, newData, newDataLen); //copy into low pass coefficients
         
         iirR.hpf.order = order;
         memcpy(iirR.hpf.coeffs, newData, newDataLen); //copy into low pass coefficients

         configureIIRChannel(iirL,HIGH_PASS,filterIn1, filterOut1, filterInt1); //configure blocks
         configureIIRChannel(iirR,HIGH_PASS,filterIn2, filterOut2, filterInt2);
       }
       
     }
     free(newData);
}

void IIRRecieveDual(int channel)
{
     int order1 = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
     int order2 = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
     int filterType = (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8];
     int* newData = (int*) malloc(shieldMailbox.inboxSize/2);  
     //Working read via serial code
     for(int i = 10; i < order1*COEFFS_PER_BIQUAD+10; i = i + 2) //copy recieved coefficients to buffer.
     {
       newData[i/2 - 5] = ((shieldMailbox.inbox[i+1]<<8) + shieldMailbox.inbox[i]);
     }
      
     
     if (channel == CHAN_LEFT) //channel 0 == left
     {
       iirL.lpf.order = order1;
       memcpy(iirL.lpf.coeffs, newData, order1/2*COEFFS_PER_BIQUAD);
     }
     else if (channel == CHAN_RIGHT) //channel 1 == right
     {
       iirR.lpf.order = order1;
       memcpy(iirR.lpf.coeffs, newData, order1/2*COEFFS_PER_BIQUAD);
     }
     else if (channel == CHAN_BOTH) //channel 2 == both
     {
       iirL.lpf.order = order1;
       memcpy(iirL.lpf.coeffs, newData, order1/2*COEFFS_PER_BIQUAD);
       
       iirR.lpf.order = order1;
       memcpy(iirR.lpf.coeffs, newData, order1/2*COEFFS_PER_BIQUAD);
     }
     
     for(int i = 10+order1*COEFFS_PER_BIQUAD; i < shieldMailbox.inboxSize; i = i + 2) //copy recieved coefficients to buffer.
     {
       newData[i/2 - 5 - order1/2*COEFFS_PER_BIQUAD] = ((shieldMailbox.inbox[i+1]<<8) + shieldMailbox.inbox[i]);
     }

     if (channel == CHAN_LEFT) //channel 0 == left
     {
       iirL.hpf.order = order2;
       memcpy(iirL.hpf.coeffs, newData, order2/2*COEFFS_PER_BIQUAD);
       configureIIRChannel(iirL,filterType,filterIn1, filterOut1, filterInt1); //configure blocks
     }
     else if (channel == CHAN_RIGHT) //channel 1 == right
     {
       iirR.hpf.order = order2;
       memcpy(iirR.hpf.coeffs, newData, order2/2*COEFFS_PER_BIQUAD);
       configureIIRChannel(iirR,filterType,filterIn2, filterOut2, filterInt2); //configure blocks
     }
     else if (channel == CHAN_BOTH) //channel 2 == both
     {
       iirL.hpf.order = order2;
       memcpy(iirL.hpf.coeffs, newData, order2/2*COEFFS_PER_BIQUAD);
       
       iirR.hpf.order = order2;
       memcpy(iirR.hpf.coeffs, newData, order2/2*COEFFS_PER_BIQUAD);

       configureIIRChannel(iirL,filterType,filterIn1, filterOut1, filterInt1); //configure blocks
       configureIIRChannel(iirR,filterType,filterIn2, filterOut2, filterInt2);

     }
     //printIIRData(iirL);
     //printIIRData(iirR);
     free(newData);
}
