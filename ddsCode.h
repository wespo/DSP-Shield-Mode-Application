//dds variables and buffers.
//OH GOD so many globals. Such variable creep. Build a struct!
//#define AUDIO_INTERRUPTION 1 //causes audio to be interrupted on SD card hit.
#ifndef _DDS_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _DDS_H_INCLUDED

#include "core.h"
#include "SD.h"
#include "Audio_exposed.h"
#include "mailbox.h"
#include <math.h>

#define CHAN_LEFT 0
#define CHAN_RIGHT 1
#define CHAN_BOTH 2

#define DDS_LENGTH 8192
#define WAV_SIN 0
#define WAV_SQU 1
#define WAV_TRI 2
#define WAV_SAW 3
#define WAV_WHT 4
#define WAV_USR 5

#define MUX_OVERWRITE 0
#define MUX_SUM 1

struct ddsConfig {
	int enable;
	unsigned long phaseAccumulator;
	long tuningIncrement;
	unsigned long tuningStart;
	unsigned long tuningStop;
	unsigned long tuningWord;
	unsigned int chirpLoopFlag;
	int gain;
	int muxMode;
	int wavType;
        int* phaseToAmplitude;
};

void ddsGen(ddsConfig &config, int buffer[], int size);

void ddsConfigInit(ddsConfig &config);

void loadWave(int type, int channel, bool doubleBuffFlag);

unsigned long computeTuningWord(float f);

long computeRamp(float fStart, float fEnd, unsigned int duration);

ddsConfig ddsInit(int command, float fStart, float fEnd, float gain, int chirpLoop, unsigned int duration, int sumMode, int type);

void ddsChirpStart(int channel, int command);
void ddsToneStart(int channel, int command);
void ddsStop(int channel);

extern int phase_to_amplitude_l[DDS_LENGTH];
extern int phase_to_amplitude_r[DDS_LENGTH];
extern ddsConfig ddsConfigLeft, ddsConfigRight;
#endif
