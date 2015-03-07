#include "filterIir.h"
#include "mailbox.h"
#include "SD.h"
#define FILTER_LENGTH_DEFAULT (201)
#define FILTER_LENGTH_MAX (511)
#define FILE_CHUNK (100)
#define MAX_IIR_FREQUENCY (20000)
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

inline void iirCoeffsPrint(iirChannel &channel)
{
  Serial.begin(115200);
  Serial.println("Loaded Coeffs:");
  for(int i = 0; i < channel.lpf.order/2*COEFFS_PER_BIQUAD; i++) //fix endian-ness of dataset.
  {
    Serial.print(channel.lpf.coeffs[i]);
    Serial.print(",");
    if((i+1)%COEFFS_PER_BIQUAD==0)
      Serial.print("\n");
  }
  Serial.end();
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

int loadfilterIIR(char *fresponse, char *fpass, int Hz, int* target, int order)
{
  if(order % 2) //if order is odd, return error, as order must be even for these IIR biquad filters.
  {
    return 0;
  }
  
  File fileHandle;
  char fileName[32];
  //int newCoeffs[FILTER_LENGTH_MAX] = {0};
  if(Hz > MAX_IIR_FREQUENCY) //if cutoff frequency exceeds file max, set cutoff to max
  {
    Hz = MAX_IIR_FREQUENCY;
  }
  
  
  long shortHz = Hz; //target frequency
  shortHz *= order/2; //convert order to # biquads. 
  shortHz *= COEFFS_PER_BIQUAD; // 2 bytes per coefficient, 7 coefficients per biquad 2nd order per biquad;

  sprintf(fileName, "iir/%s/%s/%d.iir",fresponse,fpass,order); //path to IIR filter file, e.g. iir/che/hpf/10.iir is a 10th order chebyshev high pass filter file.
  
  //debugging serial output.
  Serial.begin(115200);
  Serial.print("\n");
  Serial.print("File: ");
  Serial.println(fileName);
  Serial.print("Response: ");
  Serial.println(fresponse);
  Serial.print("Pass: ");
  Serial.println(fpass);
  Serial.print("Cutoff: ");
  Serial.println(Hz);
  Serial.print("Order: ");
  Serial.println(order);

  #ifdef AUDIO_INTERRUPTION //only if we want to interrupt filtering for faster loads.
    AudioC.detachIntr(); //turning off the audio fixes audio / sd card collision
  #endif

  //open the file
  fileHandle = SD.open(fileName, FILE_READ);

  if(fileHandle) //if file open successful
  {
    long offset; //holds variable inidicating cutoff frequency of first filter in the array
    int temp;
    fileHandle.read(&temp,1); //read the first word to get offset. Different files start at different frequencies
    offset = temp; //store in a long to allow for byte computation
    if(fpass[0] == 'l') // low pass filter
    {
      if(Hz < offset) //if less than the minimum frequncy, set to minimum frequency
      {
        shortHz = offset;
        shortHz *= order/2; //convert order to # biquads. 
        shortHz *= COEFFS_PER_BIQUAD; // 2 bytes per coefficient, 7 coefficients per biquad 2nd order per biquad;
      }
      
      //debug, report min frequency
      Serial.print("Min Filter: ");
      Serial.println(offset);
    }
    else //high pass filter
    {
      if(Hz > offset) //if cutoff > max, set to max
      {
        shortHz = offset;
        shortHz *= order/2; //convert order to # biquads. 
        shortHz *= COEFFS_PER_BIQUAD; // 2 bytes per coefficient, 7 coefficients per biquad 2nd order per biquad;
        offset = 0;
      }
    }
    
    
    //convert offset from Hz to number of words
    offset *= order/2*COEFFS_PER_BIQUAD; //number of coefficients per Hz
    offset -= 1; //account for the first word

    //debugging serial output
    Serial.print("Computed Offset: ");
    Serial.println(offset);
    Serial.print("Filter Target: ");
    Serial.println(shortHz);
    Serial.print("Seek Target: ");
    Serial.println(shortHz - offset);

    fileHandle.seek((shortHz - offset)*2);
    fileHandle.read(target, order/2*COEFFS_PER_BIQUAD); //load the data
    fileHandle.close();
    Serial.println("Coeffs:");
    for(int i = 0; i < order/2*COEFFS_PER_BIQUAD; i++) //fix endian-ness of dataset.
    {
      Serial.print(target[i]);
      Serial.print(",");
      if((i+1)%COEFFS_PER_BIQUAD==0)
        Serial.print("\n");
    }
    
    Serial.print("\n");
    Serial.end();
    #ifdef AUDIO_INTERRUPTION
        bool status = AudioC.Audio(TRUE);
        AudioC.setSamplingRate(SAMPLING_RATE_44_KHZ);
        if (status == 0)
        {
          AudioC.attachIntr(dmaIsr);
        }
    #endif
  }
  else
  {
    Serial.begin(115200);
    Serial.println("File Open Failed.");
    Serial.end();
    return 0;
  }
  Serial.end();
  return 1;
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
    channel.lpf.src = bufferint;
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
    memcpy(channel.lpf.src, channel.hpf.src, I2S_DMA_BUF_LEN);
    processIIR(channel.hpf);
    processIIR(channel.lpf);
    IIRsumChannels(channel.hpf, channel.lpf, I2S_DMA_BUF_LEN);
  }

}

void IIRsumChannels(iirConfig &one, iirConfig &two, int len) //sums the output buffers of two iirConfigs, divided by two to prevent overflows. Stores the result in one.
{
  for(int i = 0; i < len; i++)
  {
    one.dst[i] += two.dst[i];
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

void IIRLoad(int command, int channel)
{
  //grab pass, response, cutoff1   
  int pass = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
  int response = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
  int order = (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8];
  int cutoff = (shieldMailbox.inbox[11]<<8) + shieldMailbox.inbox[10];
  int target[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {
    0  };

  char *fresponse;
  if(response == TYPE_BUTTER)
  {
    fresponse = "btr";
  }
  else if(response == TYPE_BESSEL)
  {
    fresponse = "bsl";
  }
  else if(response == TYPE_ELLIP)
  {
    fresponse = "elp";
  }
  else if(response == TYPE_CHEBY)
  {
    fresponse = "che";
  }

  char *fpass;
  if(pass == HIGH_PASS)
  {
    fpass = "hpf";
  }
  else
  {
    fpass = "lpf";
  }   
  if(loadfilterIIR(fresponse, fpass, cutoff, target, order)) //low / high pass
  {
    if(pass == HIGH_PASS)
    {
      if (channel == CHAN_LEFT) //channel 0 == left
      {
        iirL.hpf.order = order;
        memcpy(iirL.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
      else if (channel == CHAN_RIGHT) //channel 1 == right
      {
        iirR.hpf.order = order;
        memcpy(iirR.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
      else if (channel == CHAN_BOTH) //channel 2 == both
      {
        iirL.hpf.order = order;
        memcpy(iirL.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);

        iirR.hpf.order = order;
        memcpy(iirR.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
    }
    else
    {
      if (channel == CHAN_LEFT) //channel 0 == left
      {
        iirL.lpf.order = order;
        memcpy(iirL.lpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
      else if (channel == CHAN_RIGHT) //channel 1 == right
      {
        iirR.lpf.order = order;
        memcpy(iirR.lpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
      else if (channel == CHAN_BOTH) //channel 2 == both
      {
        iirL.lpf.order = order;
        memcpy(iirL.lpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);

        iirR.lpf.order = order;
        memcpy(iirR.lpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
    }
  }


  if(command == 9) //band pass / stop
  {
    int cutoff = (shieldMailbox.inbox[11]<<8) + shieldMailbox.inbox[10];  //cutoff 2 for band pass / stop   
    fpass = "hpf";
    if(loadfilterIIR(fresponse, fpass, cutoff, target, order)) //low / high pass
    {
      if (channel == CHAN_LEFT) //channel 0 == left
      {
        iirL.hpf.order = order;
        memcpy(iirL.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
      else if (channel == CHAN_RIGHT) //channel 1 == right
      {
        iirR.hpf.order = order;
        memcpy(iirR.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
      else if (channel == CHAN_BOTH) //channel 2 == both
      {
        iirL.hpf.order = order;
        memcpy(iirL.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);

        iirR.hpf.order = order;
        memcpy(iirR.hpf.coeffs, target, order/2*COEFFS_PER_BIQUAD);
      }
    }
  }

  if(channel == CHAN_LEFT)
  {
    configureIIRChannel(iirL,pass,filterIn1, filterOut1, filterInt1); //configure blocks
  }
  else if(channel == CHAN_RIGHT)
  {
    configureIIRChannel(iirR,pass,filterIn2, filterOut2, filterInt2);
  }
  else if(channel == CHAN_BOTH)
  {
    configureIIRChannel(iirL,pass,filterIn1, filterOut1, filterInt1); //configure blocks
    configureIIRChannel(iirR,pass,filterIn2, filterOut2, filterInt2);
  }
  //iirCoeffsPrint(iirR);
  //iirCoeffsPrint(iirL);
}

