/*
	 Multipurpose DSP App.
	 Takes all sorts of commands from Arduino via SPI and performs signal processing.
 */
#include "Audio.h"
#include "filter.h"
#include "OLED.h"
#include "SPI.h"
#include "mailbox.h"
#include "SD.h"

//FIR Filtering
#include "filterFir.h"

//IIR Filtering
#include "filterIir.h"

//general defines
#define CHAN_LEFT 0
#define CHAN_RIGHT 1
#define CHAN_BOTH 2

//DDS include
#include "ddsCode.h"
ddsConfig ddsConfigLeft, ddsConfigRight;

#include "messageStateMachine.h" //extension for the mailbox library that enables the reading of a message one byte at a time instead of all at once.
messageStateData arduinoMessageState; //struct with state data for message state machine.

#include "fftCode.h"
fftConfig fftConfigLeft, fftConfigRight;

// Buffer to hold the Input Samples of left channel, for the FIR filter 
int filterIn1[I2S_DMA_BUF_LEN];

// Buffer to hold the Input Samples of right channel, for the FIR filter 
int filterIn2[I2S_DMA_BUF_LEN];

// Buffer to hold the Output Samples of left channel, from the FIR filter
int filterOut1[I2S_DMA_BUF_LEN];

// Buffer to hold the Output Samples of right channel, from the FIR filter
int filterOut2[I2S_DMA_BUF_LEN];

// Variable to indicate to the FIR Filtering section that the Input samples
// are ready to be filtered
unsigned short readyForFilter = 0;
// General Filter Routine variables
//  samples are available in the "filterOut" buffer
unsigned short filterBufAvailable = 0;

// Variable to switch between the data buffers of the Audio library
unsigned short writeBufIndex = 0;

//determines if copy from the codec is enabled
int inputCodec = 0;

inline void setXF(bool togg)
{
  if(togg)
    asm(" BIT(ST1, #13) = #1"); //flag that we had an incident and try to recover from it.
  else
    asm(" BIT(ST1, #13) = #0"); //flag that we had an incident and try to recover from it.
    
}

// DMA Interrupt Service Routine
interrupt void dmaIsr(void)
{
    unsigned short ifrValue;
    ifrValue = DMA.getInterruptStatus();
    if ((ifrValue >> DMA_CHAN_ReadR) & 0x01)
    {
      /* Data read from codec is copied to filter input buffers.
           Filtering is done after configuring DMA for next block of transfer
           ensuring no data loss */
      if(inputCodec == 0)
      {
            fillShortBuf(filterIn1, 0, I2S_DMA_BUF_LEN);
            fillShortBuf(filterIn2, 0, I2S_DMA_BUF_LEN);
      }
      else if(inputCodec == 1)
      {
        copyShortBuf(AudioC.audioInLeft[AudioC.activeInBuf],
                     filterIn1, I2S_DMA_BUF_LEN);
        copyShortBuf(AudioC.audioInRight[AudioC.activeInBuf],
                     filterIn2, I2S_DMA_BUF_LEN);
      }
        readyForFilter = 1;
    }
    else if ((ifrValue >> DMA_CHAN_WriteR) & 0x01)
    {
        if (filterBufAvailable)
        {
            /* Filtered buffers need to be copied to audio out buffers as
               audio library is configured for non-loopback mode */
            writeBufIndex = (AudioC.activeOutBuf == FALSE)? TRUE: FALSE;
            copyShortBuf(filterOut1, AudioC.audioOutLeft[writeBufIndex],
                         I2S_DMA_BUF_LEN);
            copyShortBuf(filterOut2, AudioC.audioOutRight[writeBufIndex],
                         I2S_DMA_BUF_LEN);
            filterBufAvailable = 0;
        }
    }

    /* Calling AudioC.isrDma() will copy the buffers to Audio Out of the Codec,
     * initiates next DMA transfers of Audio samples to and from the Codec
     */
    AudioC.isrDma();

    // Check if filter buffers are ready. No filtering required for write interrupt
    if (readyForFilter)
    {
        readyForFilter = 0;

        ddsGen(ddsConfigLeft, filterIn1, I2S_DMA_BUF_LEN);
        ddsGen(ddsConfigRight, filterIn2, I2S_DMA_BUF_LEN);
        
        if(FIRTagL && !IIRTagL) //FIR Only
        {
          // Filter Left Audio Channel
          filter_fir(filterIn1, FIRcoeffsL, filterOut1, delayBufferL, I2S_DMA_BUF_LEN, filterLen);
        }
        if(IIRTagL && !FIRTagL) //IIR Only
        {
          // Filter Left Audio Channel
          filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, filterIn1, filterOut1, IIRcoeffsL, IIRdelayBufferL, IIROrderL);
        }
        if(IIRTagL && FIRTagL) //Both
        {
          // Filter Left Audio Channel
          filter_fir(filterIn1, FIRcoeffsL, filterOut1, delayBufferL, I2S_DMA_BUF_LEN, filterLen);
          copyShortBuf(filterOut1, filterIn1, I2S_DMA_BUF_LEN); //copy back
          // Filter Audio Channels, Fernando Algorithm
          filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, filterIn1, filterOut1, IIRcoeffsL, IIRdelayBufferL, IIROrderL);
        }
        if(!FIRTagL && !IIRTagL) //No Filtering
        {
          copyShortBuf(filterIn1, filterOut1, I2S_DMA_BUF_LEN);
        }
        if(FIRTagR && !IIRTagR) //FIR Only
        {
        // Filter Right Audio Channel
        filter_fir(filterIn2, FIRcoeffsR, filterOut2, delayBufferR, I2S_DMA_BUF_LEN, filterLen);
        }
        if(IIRTagR && !FIRTagR) //IIR Only
        {
          // Filter Right Audio Channel
          filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, filterIn2, filterOut2, IIRcoeffsR, IIRdelayBufferR, IIROrderR);
          filterBufAvailable = 1;
        }
        if(IIRTagR && FIRTagR) //Both
        {
          // Filter Right Audio Channel
          filter_fir(filterIn2, FIRcoeffsR, filterOut2, delayBufferR, I2S_DMA_BUF_LEN, filterLen);
          copyShortBuf(filterOut2, filterIn2, I2S_DMA_BUF_LEN);
          //Filter Audio Channels, Fernando Algorithm
          filter_iirArbitraryOrder(I2S_DMA_BUF_LEN, filterIn2, filterOut2, IIRcoeffsR, IIRdelayBufferR, IIROrderR);
        }
        if(!FIRTagR && !IIRTagR) //No Filtering
        {
          copyShortBuf(filterIn2, filterOut2, I2S_DMA_BUF_LEN);
        }
        filterBufAvailable = 1;
        updateSpectrumPointer(fftConfigLeft, filterIn1, filterOut1, (int **) AudioC.audioInLeft); //pick the left source buffer
        spectrum(fftConfigLeft); //generate FFT'd spectrum
        updateSpectrumPointer(fftConfigRight, filterIn2, filterOut2, (int **) AudioC.audioInRight); //pick the right source buffer
        spectrum(fftConfigRight); //generate FFT'd spectrum
    }
}
// Initializes OLED and Audio modules 
void setup()
{
    int status;
    int index;

    pinMode(LED0, OUTPUT);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(ARD_I2C_EN, OUTPUT);
    digitalWrite(ARD_I2C_EN, LOW);
    digitalWrite(LED0, LOW);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    //Initialize OLED module for status display	
    disp.oledInit();
    disp.clear();
    disp.flip();    
    disp.setline(1);
    disp.print("Shield App");
    
    // Clear all the data buffers
    fillShortBuf(filterIn1, 0, I2S_DMA_BUF_LEN);    
    fillShortBuf(filterIn2, 0, I2S_DMA_BUF_LEN);
    fillShortBuf(filterOut1, 0, I2S_DMA_BUF_LEN);
    fillShortBuf(filterOut2, 0, I2S_DMA_BUF_LEN);

    /* Clear the delay buffers, which will be used by the FIR filtering
       algorithm, These buffers need to be initialized to all zeroes in the
       beginning of the FIR filtering algorithm */
    fillShortBuf(delayBufferL, 0, (FILTER_LENGTH_MAX + 2));       
    fillShortBuf(delayBufferR, 0, (FILTER_LENGTH_MAX + 2));       
    
    /* Audio library is configured for non-loopback mode. Gives enough time for
       FIR filter processing in ISR */
    shieldMailbox.begin(SPI_MASTER, readFilter);
    
    //initialize SD Card
    status = SD.begin(1);
    status = AudioC.Audio(TRUE);
    AudioC.setSamplingRate(SAMPLING_RATE_44_KHZ);
    if (status == 0)
    {
        AudioC.attachIntr(dmaIsr);
    }  
    AudioC.setOutputVolume(94);
    //shieldMailbox.begin(SPI_MASTER, readFilter);

    //initialize DDS module for both left and right channels
    ddsConfigInit(ddsConfigLeft);
    ddsConfigInit(ddsConfigRight);
    fftConfigLeft = FFTInit();
    fftConfigRight = FFTInit();

    //initialize mailbox per byte machine.
    arduinoMessageState = initMessageStateData(arduinoMessageState);
   
}
#define tDelay 20
int heartbeat = 0;
void loop()
{
  if(heartbeat == 1)
  {
    heartbeat = 0;
  }
  else
  {
    heartbeat = 1;
  }
  digitalWrite(LED1, heartbeat);
  
  if(messageStateMachine(arduinoMessageState))
  {
    readFilter();
  }
  sendSpectrum(fftConfigLeft); //send the spectrum if needed.
  sendSpectrum(fftConfigRight);
  delayMicroseconds(10);
}

int ledBlink = 0;
void readFilter()
{
   //first get the key tags that are present in every message
   int command = (shieldMailbox.inbox[1]<<8) + shieldMailbox.inbox[0];
   int channel = (shieldMailbox.inbox[3]<<8) + shieldMailbox.inbox[2];
   bool switch1;
   int* newData;
   char fType[4] = "fir";
   char fPass[4] = "lpf";  
   int fCutoff, fQuality, sumMode, type;
   ddsConfig newConfig;
   float fDDS, gain;
   long recon;
   //parse the command, based on which command we recieved.
   switch(command) {
   case 1: //fir load direct, syntax is: <int command><FILTER_LENGTH x int coefficients>
     newData = (int*) malloc(shieldMailbox.inboxSize/2);  
     //Working read via serial code
     for(int i = 4; i < shieldMailbox.inboxSize; i = i + 2)
     {
       newData[i/2 - 2] = ((shieldMailbox.inbox[i+1]<<8) + shieldMailbox.inbox[i]);
     }     

     int filterLenNew = shieldMailbox.inboxSize/2 - 2; //command and channel take up 4 words the rest are taps.
     if(filterLenNew > 511) //max out at 511 taps, the absolute most we dare handle.
     {
       filterLenNew = 511;
     }

     if (channel == CHAN_LEFT) //channel 0 == left
     {
       memcpy(FIRcoeffsL, newData, filterLen);
       FIRTagL = 1;
     }
     else if (channel == CHAN_RIGHT) //channel 1 == right
     {
       memcpy(FIRcoeffsR, newData, filterLen);
       FIRTagR = 1;
     }
     else if (channel == CHAN_BOTH) //channel 2 == both
     {
       memcpy(FIRcoeffsL, newData, filterLen);
       memcpy(FIRcoeffsR, newData, filterLen);       
       FIRTagL = 1;
       FIRTagR = 1;
     }

     free(newData);
     break;
   case 2: //2 = FIR LPF, 3 = FIR HPF, syntax is: <int command><int cutoff>
   case 3: 
     //get Fc
     fCutoff = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];

     fQuality = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6]; //quality indicator for filter load.
     
     //note, we do this check for sanitization.
     if((fQuality == 41) || (fQuality == 101) || (fQuality == 201) || (fQuality == 301) || (fQuality == 401) || (fQuality == 511)) //only pass qualities that are on the SD card
     {
        filterLen = fQuality;
     }
     else
     {
        filterLen = 201; //default to medium quality filter
     }

     if(command == 3) //hpf
     {
       fPass[0] = 'h';  // HPF, not LPF
     }
     //High or low pass
     newData = (int*) malloc(filterLen);  
     loadfilter(fType, fPass, fCutoff, newData, filterLen);
     if (channel == CHAN_LEFT) //channel 0 == left
     {
       memcpy(FIRcoeffsL, newData, filterLen);
       FIRTagL = 1;
     }
     else if (channel == CHAN_RIGHT) //channel 1 == right
     {
       memcpy(FIRcoeffsR, newData, filterLen);
       FIRTagR = 1;       
     }
     else if (channel == CHAN_BOTH) //channel 2 == both
     {
       memcpy(FIRcoeffsL, newData, filterLen);
       memcpy(FIRcoeffsR, newData, filterLen);       
       FIRTagL = 1;
       FIRTagR = 1;       
     }
     free(newData);
     break;
   case 4: //4 = FIR BANDPASS, 5 = FIR NOTCH, syntax is: <int command><int lower bound><int upper bound>
   case 5:
      filterLen = FILTER_LENGTH_DEFAULT;
     //BAND FILTER
     int fCutoff2 = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4]; //lpf
     int fCutoff1 = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6]; //hpf

     fQuality = (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8]; //quality indicator for filter load.
     
     //note, we do this check for sanitization.
     if((fQuality == 41) || (fQuality == 101) || (fQuality == 201) || (fQuality == 301) || (fQuality == 401) || (fQuality == 511)) //only pass qualities that are on the SD card
     {
        filterLen = fQuality;
     }
     else
     {
        filterLen = 201; //default to medium quality filter
     }

     int coeffs1[2*FILTER_LENGTH_MAX-1] = {0};
     int coeffs2[FILTER_LENGTH_MAX] = {0};   


     loadfilter(fType, fPass, fCutoff1, coeffs1 + filterLen/2, filterLen);
     fPass[0] = 'h'; // HPF, not LPF
     loadfilter(fType, fPass, fCutoff2, coeffs2, filterLen);

     //for band, we cascade the filters via convolution.
     convol((DATA*)coeffs1, (DATA*)coeffs2, (DATA*)coeffs1, filterLen, filterLen);
     if(command == 5) //if notch, flip the band filter.
     {
       for(int i = 0; i < filterLen; i++)
       {
         coeffs1[i] *= -1;// * (coeffs2[i] + coeffs1[i]);
       }
       coeffs1[filterLen/2]+=32767;
     }
     if (channel == CHAN_LEFT) //channel 0 == left
     {
       memcpy(FIRcoeffsL, coeffs1, filterLen);
       FIRTagL = 1;
     }
     else if (channel == CHAN_RIGHT) //channel 1 == right
     {
       memcpy(FIRcoeffsR, coeffs1, filterLen);
       FIRTagR = 1;
     }
     else if (channel == CHAN_BOTH) //channel 2 == both
     {
       memcpy(FIRcoeffsL, coeffs1, filterLen);
       memcpy(FIRcoeffsR, coeffs1, filterLen);       
       FIRTagL = 1;
       FIRTagR = 1;
     }
     break;
   case 6: //FIR disable.
     if(channel == CHAN_LEFT)
     {
       FIRTagL = 0;
     }
     else if(channel == CHAN_RIGHT)
     {
       FIRTagR = 0;       
     }
     else if(channel == CHAN_BOTH)
     {
       FIRTagL = 0;
       FIRTagR = 0;
     }
     break;
   case 7:
     int order = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
     newData = (int*) malloc(shieldMailbox.inboxSize/2);  
     //Working read via serial code
     for(int i = 6; i < shieldMailbox.inboxSize; i = i + 2)
     {
       newData[i/2 - 3] = ((shieldMailbox.inbox[i+1]<<8) + shieldMailbox.inbox[i]);
     }
     if (channel == CHAN_LEFT) //channel 0 == left
     {
       IIROrderL = order;
       memcpy(IIRcoeffsL, newData, order/2*COEFFS_PER_BIQUAD);
       IIRTagL = 1;
     }
     else if (channel == CHAN_RIGHT) //channel 1 == right
     {
       IIROrderR = order;
       memcpy(IIRcoeffsR, newData, order/2*COEFFS_PER_BIQUAD);
       IIRTagR = 1;
     }
     else if (channel == CHAN_BOTH) //channel 2 == both
     {
       IIROrderL = order;
       IIROrderR = order;
       memcpy(IIRcoeffsL, newData, order/2*COEFFS_PER_BIQUAD);
       memcpy(IIRcoeffsR, newData, order/2*COEFFS_PER_BIQUAD);       
       IIRTagL = 1;
       IIRTagR = 1;
     }
     free(newData);
     break;
   case 8: //IIR Load. Next byte is Type (butter, bessel, etc) then following byte is Pass (high low etc))
      //currently this is a do-nothing function.
     IIRTagL = 1;
     IIRTagR = 1;
     break;   
   case 9: //IIR toggle
      if(IIRTagL == 0)
       IIRTagL = 1;
     else
       IIRTagL = 0;    
      if(IIRTagL == 0)
       IIRTagR = 1;
     else
       IIRTagR = 0;    
     break;
   case 10: //initialize DDS
     //parse out two floats, frequency and gain from mailbox.
     recon = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
     recon <<= 16;
     recon += (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
     memcpy(&fDDS, &recon, sizeof(fDDS));
     recon = (shieldMailbox.inbox[11]<<8) + shieldMailbox.inbox[10];
     recon <<= 16;
     recon += (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8];
     memcpy(&gain, &recon, sizeof(gain));
     sumMode = (shieldMailbox.inbox[13]<<8) + shieldMailbox.inbox[12];
     type = (shieldMailbox.inbox[15]<<8) + shieldMailbox.inbox[14];
     switch1 = (shieldMailbox.inbox[17]<<8) + shieldMailbox.inbox[16];
       newConfig = ddsInit(command, fDDS, fDDS, gain, 0, 0, sumMode, type); //ddsInit builds a configuration
       loadWave(newConfig.wavType, channel, switch1); //fills phase to amplitude buffer.
       if(channel == CHAN_LEFT)
       {
        ddsConfigLeft = newConfig;
       }
       else if(channel == CHAN_RIGHT)
       {
        newConfig.phaseToAmplitude = phase_to_amplitude_r;
        ddsConfigRight = newConfig;
       }
       else if(channel == CHAN_BOTH)
       {
        ddsConfigLeft = newConfig;
        newConfig.phaseToAmplitude = phase_to_amplitude_r;
        memcpy(phase_to_amplitude_r, phase_to_amplitude_l, DDS_LENGTH); //if both channels, we have to copy the buffer over.
        ddsConfigRight = newConfig;
       }
     break;
   case 11: //DDS Stop.
     if(channel == CHAN_LEFT)
     {
       ddsConfigLeft.enable = 0;
     }
     else if(channel == CHAN_RIGHT)
     {
       ddsConfigRight.enable = 0;
     }
     else if(channel == CHAN_BOTH)
     {
       ddsConfigLeft.enable = 0;
       ddsConfigRight.enable = 0;
     }
     break;
   case 12: //DDS chirp Start. Takes frequency, channel, amplitude.
     float fDDSEnd;
     //parse out two floats, frequency and gain from mailbox.
     recon = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
     recon <<= 16;
     recon += (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
     memcpy(&fDDS, &recon, sizeof(fDDS));
     recon = (shieldMailbox.inbox[11]<<8) + shieldMailbox.inbox[10];
     recon <<= 16;
     recon += (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8];
     memcpy(&fDDSEnd, &recon, sizeof(fDDSEnd));
     recon = (shieldMailbox.inbox[15]<<8) + shieldMailbox.inbox[14];
     recon <<= 16;
     recon += (shieldMailbox.inbox[13]<<8) + shieldMailbox.inbox[12];
     memcpy(&gain, &recon, sizeof(gain));     
     int duration = (shieldMailbox.inbox[17]<<8) + shieldMailbox.inbox[16];
     int chirpLoop = (shieldMailbox.inbox[19]<<8) + shieldMailbox.inbox[18];
     int sumMode = (shieldMailbox.inbox[21]<<8) + shieldMailbox.inbox[20];
     int type = (shieldMailbox.inbox[23]<<8) + shieldMailbox.inbox[22];
     switch1 = (shieldMailbox.inbox[25]<<8) + shieldMailbox.inbox[24];
       //store the new configuration in selected channel(s)
       newConfig = ddsInit(command, fDDS, fDDSEnd, gain, chirpLoop, duration, sumMode, type);
       loadWave(newConfig.wavType, channel, switch1); //fills phase to amplitude buffer.
       if(channel == CHAN_LEFT)
       {
        ddsConfigLeft = newConfig;
       }
       else if(channel == CHAN_RIGHT)
       {
        newConfig.phaseToAmplitude = phase_to_amplitude_r;
        ddsConfigRight = newConfig;
       }
       else if(channel == CHAN_BOTH)
       {
        ddsConfigLeft = newConfig;
        newConfig.phaseToAmplitude = phase_to_amplitude_r;
        memcpy(phase_to_amplitude_r, phase_to_amplitude_l, DDS_LENGTH); //if both channels, we have to copy the buffer over.
        ddsConfigRight = newConfig;
       }
       break;
   case 13: //spectrum initialize
     setXF(true);
     unsigned int interval = (shieldMailbox.inbox[5]<<8) + shieldMailbox.inbox[4];
     unsigned int source = (shieldMailbox.inbox[7]<<8) + shieldMailbox.inbox[6];
     int complex, numPoints, windowType;
     if(shieldMailbox.inboxSize > 8)
     {
        complex = (shieldMailbox.inbox[9]<<8) + shieldMailbox.inbox[8];
        numPoints = (shieldMailbox.inbox[11]<<8) + shieldMailbox.inbox[10];
        windowType = (shieldMailbox.inbox[13]<<8) + shieldMailbox.inbox[12];
     }
     else
     {
        complex = SPECTRUM_MAGNITUDE;
        numPoints = I2S_DMA_BUF_LEN;
        windowType = WINDOW_BLACKMAN;
     }
     if(channel == CHAN_LEFT)
     {
        fftConfigLeft = configFFT(1, interval, source, channel, complex,  windowType, numPoints); //configures an FFTConfig;
     }
     else if(channel == CHAN_RIGHT)
     {
       fftConfigRight = configFFT(1, interval, source, channel, complex,  windowType, numPoints); //configures an FFTConfig;
     }
     else if(channel == CHAN_BOTH)
     {
        fftConfigLeft = configFFT(1, interval, source, CHAN_LEFT, complex,  windowType, numPoints); //configures an FFTConfig
        fftConfigRight = configFFT(1, interval, source, CHAN_RIGHT, complex,  windowType, numPoints); //configures an FFTConfig
     }
     break;
   case 14: //diable spectrum;
     if(channel == CHAN_LEFT)
     {
       fftConfigLeft.enable = 0;
     }
     else if(channel == CHAN_RIGHT)
     {
       fftConfigRight.enable = 0;
     }
     else if(channel == CHAN_BOTH)
     {
       fftConfigLeft.enable = 0;
       fftConfigRight.enable = 0;
     }
     break;
   case 15: //enable audio codec input
     inputCodec = 1;
     break;
   case 16: //diable audio codec input
     inputCodec = 0;
     break;
   
   }
  //friendly messaged recieve LED toggle.
  if(ledBlink)
  {
    ledBlink = 0;
  }
  else
  {
    ledBlink = 1;
  }
  digitalWrite(LED0, ledBlink);
}
