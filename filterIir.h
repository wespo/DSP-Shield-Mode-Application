//IIR Buffers
#define IIR_ORDER_MAX    (40)
#define DELAY_COUNT      (5)
#define COEFFS_PER_BIQUAD (7)
#define IIR_DELAY_BUF_SIZE   ((IIR_ORDER_MAX/2)*(DELAY_COUNT))

volatile int IIRTagL = 0;
volatile int IIRTagR = 0;
#pragma DATA_ALIGN(32);
int IIRcoeffsL[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
int IIRGainL = 1;
int IIRcoeffsR[COEFFS_PER_BIQUAD*IIR_ORDER_MAX/2] = {0};
int IIRGainR = 1;
int IIROrderL = 0;
int IIROrderR = 0;
// Delay buffer used by the IIR filtering algorithm for Left Channel
#pragma DATA_ALIGN(32);
long IIRdelayBufferL[IIR_DELAY_BUF_SIZE] = {0};

// Delay buffer used by the IIR filtering algorithm for Right Channel
#pragma DATA_ALIGN(32);
long IIRdelayBufferR[IIR_DELAY_BUF_SIZE] = {0};
void loadfilterIIR(char* ftype, char *fresponse, char *fpass, int Hz, int* target, int order)
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
    sprintf(fileName, "%s/%d/%s/%d.flt",ftype,fresponse,order,fpass,fileBin); //default file name
    
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

