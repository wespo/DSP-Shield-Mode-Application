#include "filterIir.h"

#define FILTER_LENGTH_DEFAULT (201)
#define FILTER_LENGTH_MAX (511)
#define FILE_CHUNK (100)

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

iirConfig initIIR(int *src, int *dst)
{
  return(initIIR(0,0));
}
iirConfig initIIR()
{
  iirConfig config;
  config.src = 0;
  config.dst = 0;
  config.coeffs = 0;
  config.delayBuf = 0;
  config.order = 0;
  config.enabled = 0;
  return config;
}

void processIIR(iirConfig &config)
{
  if(config.enabled)
  {
    filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, config.src, config.dst, config.coeffs, config.delayBuf, config.order);  
  }
  
}
