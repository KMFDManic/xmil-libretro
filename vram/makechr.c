#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"vram.h"
#include	"makesub.h"
#include	"font.h"


extern	BYTE	blinktest;


UINT32 makechr8(UINT8 *dst, UINT pos, UINT count, REG8 udtmp) {

	REG8		atr;
	REG8		ank;
	REG8		knj;
const UINT8		*pat;
	MAKETXTFN	fn;

	atr = tram[TRAM_ATR + pos];
	if (atr & blinktest) {
		atr ^= X1ATR_REVERSE;
	}
	if (udtmp & 0x10) {
		pos = LOW11(pos - 1);
	}
	ank = tram[TRAM_ANK + pos];
	knj = tram[TRAM_KNJ + pos];
	if (!(tram[TRAM_ATR + pos] & 0x20)) {		// CHR
		if (!(knj & 0x80)) {					// ASCII
			pat = font_ank + (ank << 3);
			fn = maketxt8fn[udtmp & 15];
		}
		else {									// KANJI
			pat = font_knjx1t;
			pat += (knj & 0x1f) << 12;
			pat += ank << 4;
			if (knj & 0x40) {
				pat += FONTX1T_LR;
			}
			fn = makeknj8fn[udtmp & 15];
		}
		(*fn)(dst, dst + count, pat);
		(*makeatr8[atr & 15])(dst, dst + count);
		return(0x40404040);
	}
	else {										// PCG
		if (!(knj & 0x90)) {					// PCG�̏o��
			pat = pcg.d.b[ank];
			fn = maketxt8fn[udtmp & 15];
		}
		else {									// 16�h�b�gPCG�̏o��
			pat = pcg.d.b[ank & (~1)];
			fn = makeknj8fn[udtmp & 15];
		}
		makeatr_pcg8(dst, count, pat, atr, fn);
		return(0x80808080);
	}
}

UINT32 makechr16(UINT8 *dst, UINT pos, UINT count, REG8 udtmp) {

	REG8		atr;
	REG8		ank;
	REG8		knj;
const UINT8		*pat;
	MAKETXTFN	fn;

	atr = tram[TRAM_ATR + pos];
	if (atr & blinktest) {
		atr ^= X1ATR_REVERSE;
	}
	if (udtmp & 0x10) {
		pos = LOW11(pos - 1);
	}
	ank = tram[TRAM_ANK + pos];
	knj = tram[TRAM_KNJ + pos];
	if (!(tram[TRAM_ATR + pos] & 0x20)) {		// CHR
		if (!(knj & 0x80)) {					// ASCII
			pat = font_txt;
		}
		else {									// KANJI
			pat = font_knjx1t;
			pat += (knj & 0x1f) << 12;
			if (knj & 0x40) {
				pat += FONTX1T_LR;
			}
		}
		fn = maketxt16fn[udtmp & 15];
		(*fn)(dst, dst + count, pat + (ank << 4));
		(*makeatr8[atr & 15])(dst, dst + count);
		(*makeatr8[atr & 15])(dst + MAKETEXT_STEP,
								dst + MAKETEXT_STEP + count);
		return(0x40404040);
	}
	else {										// PCG
		if (!(knj & 0x90)) {					// PCG�̏o��
			pat = pcg.d.b[ank];
			fn = makepcg16fn[udtmp & 15];
		}
		else {									// 16�h�b�gPCG�̏o��
			pat = pcg.d.b[ank & (~1)];
			fn = maketxt16fn[udtmp & 15];
		}
		makeatr_pcg16(dst, count, pat, atr, fn);
		return(0x80808080);
	}
}
