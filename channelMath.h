#ifndef _CHANMATH_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _CHANMATH_H_INCLUDED

struct mathChannelConfig {
	int modeL;
        int modeR;
	int* inL;
	int* inR;
	int* outL;
	int* outR;
};

#define MATH_NONE   0
#define MATH_SUB    1
#define MATH_ADD    2

void mathChannelInit(int* inL, int* outL, int* inR, int* outR, mathChannelConfig &config);

void mathChannelMode(mathChannelConfig &config, int channel, int mode);

void processMathChannels(mathChannelConfig &config);

void processMathChannel(int* src1, int* src2, int* dst, int mode);

#endif
