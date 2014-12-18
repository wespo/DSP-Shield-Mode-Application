// Length of the Coefficient Vector
//#define AUDIO_INTERRUPTION 1 //causes audio to be interrupted on SD card hit.
#define FILTER_LENGTH_DEFAULT (201)
#define FILTER_LENGTH_MAX (511)
#define FILE_CHUNK (100)

//general defines
#define CHAN_LEFT 0
#define CHAN_RIGHT 1
#define CHAN_BOTH 2

#define LOW_PASS 1
#define HIGH_PASS 2
#define BAND_PASS 3
#define BAND_STOP 4

unsigned int filterLen = FILTER_LENGTH_DEFAULT;
//fir buffers
volatile int FIRTagL = 0;
volatile int FIRTagR = 0;
// FIR Filter coefficients with 201 taps
int FIRcoeffsL[FILTER_LENGTH_MAX] = {0};
int FIRcoeffsR[FILTER_LENGTH_MAX] = {0};
// Delay buffer used by the FIR filtering algorithm for Left Channel
int delayBufferL[FILTER_LENGTH_MAX + 2];
// Delay buffer used by the FIR filtering algorithm for Right Channel
int delayBufferR[FILTER_LENGTH_MAX + 2];

extern interrupt void dmaIsr(void);

void loadfilter(char* ftype, char *fpass, int Hz, int* target, int targetLength)
{
    File          fileHandle;
    //int newCoeffs[FILTER_LENGTH_MAX] = {0};
    
    long shortHz = Hz;
    shortHz -= 10; //lowest cutoff frequency = 10Hz
    int fileBin = shortHz / FILE_CHUNK; //alias to 1000Hz files.
    shortHz %= FILE_CHUNK; //alias to 1000Hz bins.
    shortHz *= targetLength; //201 tap long filters;
    shortHz *= 2; // 2 bytes per tap;
    char fileName[32];
    sprintf(fileName, "%s/%d/%s/%d.flt",ftype,targetLength,fpass,fileBin); //default file name
    
//    for(int i = 0; i<3; i++)
//    {
//      fileName[i] = ftype[i];
//      fileName[i+4] = fpass[i];
//    }
//    fileName[0] = ftype[0];
//    fileName[4] = fpass[0];
      
//    Serial.begin(115200);    
//    Serial.print("Freq: ");
//    Serial.println(Hz);
//    Serial.print("Taps: ");
//    Serial.println(targetLength);
//    Serial.println(fileName);
//    Serial.end();

    //if (TRUE == status)
    if(1)
     {
       #ifdef AUDIO_INTERRUPTION
       AudioC.detachIntr(); //turning off the audio fixes audio / sd card collision
       #endif
       fileHandle = SD.open(fileName, FILE_READ);
        if(fileHandle)
        {

          fileHandle.seek(shortHz);
//        for(int i = 0; i < targetLength; i++)
//        {
//          int coeff;
//          fileHandle.read((char*) &coeff, 1);
//          int coeff2;
//          fileHandle.read((char*) &coeff2, 1);
//          coeff = (coeff2<<8) + coeff;
//          target[i] = coeff;
//        }
         fileHandle.read(target, targetLength); //load the data
         fileHandle.close();
        
          for(int i = 0; i < targetLength; i++) //fix endian-ness of dataset.
         {
           int temp = target[i];
           target[i] = ((temp & 0x00FF)<<8) + ((temp & 0xFF00)>>8);
         }          
         //memcpy(target, newCoeffs, targetLength);
        }
//        else
//        {
////          Serial.begin(115200);    
////          Serial.println("File Open Failed");
////          Serial.end();
//        }
       #ifdef AUDIO_INTERRUPTION
       bool status = AudioC.Audio(TRUE);
       AudioC.setSamplingRate(SAMPLING_RATE_44_KHZ);
       if (status == 0)
       {
         AudioC.attachIntr(dmaIsr);
       }
       #endif

//   else //failed file load LED Flag.
//   {
//       asm(" BIT(ST1, #13) = #1"); //flag that we had an incident and try to recover from it.
//   }
     }
}

void firDisable(int channel)
{
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
}
