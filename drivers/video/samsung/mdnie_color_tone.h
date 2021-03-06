#ifndef __MDNIE_COLOR_TONE_H__
#define __MDNIE_COLOR_TONE_H__

#include "mdnie.h"

static const unsigned short tune_color_tone_1[] = {
	//start
	0x0001, 0x0040,	//SCR HDTR
	0x002c, 0x0fff,	//DNR bypass 0x003C
	0x002d, 0x1900,	//DNR bypass 0x0a08
	0x002e, 0x0000,	//DNR bypass 0x1010
	0x002f, 0x0fff,	//DNR bypass 0x0400
	0x003a, 0x0009,	//HDTR CS
	0x003f, 0x0000,	//CS GAIN
	0x00c8, 0x0000,	//kb R	SCR
	0x00c9, 0x0000,	//gc R
	0x00ca, 0xafaf,	//rm R
	0x00cb, 0xafaf,	//yw R
	0x00cc, 0x0000,	//kb G
	0x00cd, 0xb7b7,	//gc G
	0x00ce, 0x0000,	//rm G
	0x00cf, 0xb7b7,	//yw G
	0x00d0, 0x00bc,	//kb B
	0x00d1, 0x00bc,	//gc B
	0x00d2, 0x00bc,	//rm B
	0x00d3, 0x00bc,	//yw B
	0x00d6, 0x3f00,	//GAMMA bp2
	0x00d7, 0x2003,
	0x00d8, 0x2003,
	0x00d9, 0x2003,
	0x00da, 0x2003,
	0x00db, 0x2003,
	0x00dc, 0x2003,
	0x00dd, 0x2003,
	0x00de, 0x2003,
	0x00df, 0x2003,
	0x00e0, 0x2003,
	0x00e1, 0x2003,
	0x00e2, 0x2003,
	0x00e3, 0x2003,
	0x00e4, 0x2003,
	0x00e5, 0x2003,
	0x00e6, 0x2003,
	0x00e7, 0x2003,
	0x00e8, 0x2003,
	0x00e9, 0x2003,
	0x00ea, 0x2003,
	0x00eb, 0x2003,
	0x00ec, 0x2003,
	0x00ed, 0xff00,
	0x00d5, 0x0001,
	0x0028, 0x0000,	//Register Mask
	//end
	END_SEQ, 0x0000,
};

static const unsigned short tune_color_tone_2[] = {
	//start
	0x0001, 0x0040,	//SCR HDTR
	0x002c, 0x0fff,	//DNR bypass 0x003C
	0x002d, 0x1900,	//DNR bypass 0x0a08
	0x002e, 0x0000,	//DNR bypass 0x1010
	0x002f, 0x0fff,	//DNR bypass 0x0400
	0x003a, 0x0009,	//HDTR CS
	0x003f, 0x0000,	//CS GAIN
	0x00c8, 0x0000,	//kb R	SCR
	0x00c9, 0x0000,	//gc R
	0x00ca, 0xa0a0,	//rm R
	0x00cb, 0xa0a0,	//yw R
	0x00cc, 0x0000,	//kb G
	0x00cd, 0xa8a8,	//gc G
	0x00ce, 0x0000,	//rm G
	0x00cf, 0xa8a8,	//yw G
	0x00d0, 0x00b2,	//kb B
	0x00d1, 0x00b2,	//gc B
	0x00d2, 0x00b2,	//rm B
	0x00d3, 0x00b2,	//yw B
	0x00d6, 0x3f00,	//GAMMA bp2
	0x00d7, 0x2003,
	0x00d8, 0x2003,
	0x00d9, 0x2003,
	0x00da, 0x2003,
	0x00db, 0x2003,
	0x00dc, 0x2003,
	0x00dd, 0x2003,
	0x00de, 0x2003,
	0x00df, 0x2003,
	0x00e0, 0x2003,
	0x00e1, 0x2003,
	0x00e2, 0x2003,
	0x00e3, 0x2003,
	0x00e4, 0x2003,
	0x00e5, 0x2003,
	0x00e6, 0x2003,
	0x00e7, 0x2003,
	0x00e8, 0x2003,
	0x00e9, 0x2003,
	0x00ea, 0x2003,
	0x00eb, 0x2003,
	0x00ec, 0x2003,
	0x00ed, 0xff00,
	0x00d5, 0x0001,
	0x0028, 0x0000,	//Register Mask
	//end
	END_SEQ, 0x0000,
};

static const unsigned short tune_color_tone_3[] = {
	//start
	0x0001, 0x0040,	//SCR HDTR
	0x002c, 0x0fff,	//DNR bypass 0x003C
	0x002d, 0x1900,	//DNR bypass 0x0a08
	0x002e, 0x0000,	//DNR bypass 0x1010
	0x002f, 0x0fff,	//DNR bypass 0x0400
	0x003a, 0x0009,	//HDTR CS
	0x003f, 0x0000,	//CS GAIN
	0x00c8, 0x0000,	//kb R	SCR
	0x00c9, 0x0000,	//gc R
	0x00ca, 0x9191,	//rm R
	0x00cb, 0x9191,	//yw R
	0x00cc, 0x0000,	//kb G
	0x00cd, 0x9999,	//gc G
	0x00ce, 0x0000,	//rm G
	0x00cf, 0x9999,	//yw G
	0x00d0, 0x00a3,	//kb B
	0x00d1, 0x00a3,	//gc B
	0x00d2, 0x00a3,	//rm B
	0x00d3, 0x00a3,	//yw B
	0x00d6, 0x3f00,	//GAMMA bp2
	0x00d7, 0x2003,
	0x00d8, 0x2003,
	0x00d9, 0x2003,
	0x00da, 0x2003,
	0x00db, 0x2003,
	0x00dc, 0x2003,
	0x00dd, 0x2003,
	0x00de, 0x2003,
	0x00df, 0x2003,
	0x00e0, 0x2003,
	0x00e1, 0x2003,
	0x00e2, 0x2003,
	0x00e3, 0x2003,
	0x00e4, 0x2003,
	0x00e5, 0x2003,
	0x00e6, 0x2003,
	0x00e7, 0x2003,
	0x00e8, 0x2003,
	0x00e9, 0x2003,
	0x00ea, 0x2003,
	0x00eb, 0x2003,
	0x00ec, 0x2003,
	0x00ed, 0xff00,
	0x00d5, 0x0001,
	0x0028, 0x0000,	//Register Mask
	//end
	END_SEQ, 0x0000,
};

struct mdnie_tunning_info tune_color_tone[3] = {
	{"COLOR_TONE_1",	tune_color_tone_1},
	{"COLOR_TONE_2",	tune_color_tone_2},
	{"COLOR_TONE_3",	tune_color_tone_3},
};


#endif /* __MDNIE_COLOR_TONE_H__ */
