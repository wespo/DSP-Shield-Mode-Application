//dds variables and buffers.
//OH GOD so many globals. Such variable creep. Build a struct!
//#define AUDIO_INTERRUPTION 1 //causes audio to be interrupted on SD card hit.
#ifndef _NOISE_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _NOISE_H_INCLUDED

#include "core.h"
#include "SD.h"
#include "Audio_exposed.h"
#include "mailbox.h"
#include <math.h>
#include "DSPLIB.h"

#define CHAN_LEFT 0
#define CHAN_RIGHT 1
#define CHAN_BOTH 2

#define MUX_OVERWRITE 0
#define MUX_SUM 1

struct noiseConfig {
	int enable;
	int gain;
	int muxMode;
};

void noiseGen(noiseConfig &config, int buffer[], int frameSize);

void noiseConfigInit(noiseConfig &config);

noiseConfig noiseInit(float gain, int sumMode);

void noiseStart(int channel);
void noiseStop(int channel);
void gain(int gain, int buffer[], int frameSize);

extern noiseConfig noiseConfigLeft, noiseConfigRight;

#endif
