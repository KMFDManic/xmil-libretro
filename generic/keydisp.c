/**
 * @file	keydisp.c
 * @brief	Implementation of the key display
 */

#include "compiler.h"

#if defined(SUPPORT_KEYDISP)

#include "keydisp.h"
#include <math.h>
#include "pccore.h"
#include "iocore.h"
#include "sound/psggen.h"

typedef struct
{
	UINT8	k[KEYDISP_NOTEMAX];
	UINT8	r[KEYDISP_NOTEMAX];
	UINT	remain;
	UINT8	flag;
	UINT8	padding[3];
} KDCHANNEL;

typedef struct
{
	UINT8	ch;
	UINT8	key;
} KDDELAYE;

typedef struct
{
	UINT	pos;
	UINT	rem;
	UINT8	warm;
	UINT8	warmbase;
} KDDELAY;

/**
 * @brief Channel
 */
struct tagFmChannel
{
	UINT16 nFNumber;				/*!< F-Number */
	UINT8 cLastNote;				/*!< The last note */
	UINT8 cKeyOn;					/*!< KeyOn */
};
typedef struct tagFmChannel		FMCHANNEL;

/**
 * @brief OPM
 */
struct tagOpmControl
{
	const UINT8 *pcRegister;		/*!< The pointer of the register */
	UINT8 cChannelNum;				/*!< The number of the channel */
	UINT8 cFMChannels;				/*!< The channels of FM */
	SINT16 wTone;					/*!< The Tone */
	FMCHANNEL ch[8];				/*!< The information of FM */
};
typedef struct tagOpmControl	OPMCTL;

/**
 * @brief PSG
 */
struct tagPsgControl
{
	const UINT8 *pcRegister;		/*!< The pointer of the register */
	REG16 nLastTone[4];
	UINT8 cLastNote[4];
	UINT16 wTone[13];				/*!< The list of Tone */
	UINT8 cChannelNum;				/*!< The number of the channel */
	UINT8 cPsgOn;
	UINT8 cLastMixer;
};
typedef struct tagPsgControl	PSGCTL;

typedef struct
{
	UINT8		mode;
	UINT8		dispflag;
	UINT8		framepast;
	UINT8		keymax;
	UINT8		opmmax;
	UINT8		psgmax;
	KDDELAY		delay;
	KDCHANNEL	ch[KEYDISP_CHMAX];
	OPMCTL		opmctl[2];			/*!< OPM */
	PSGCTL		psgctl[1];			/*!< PSG */
	KDDELAYE	delaye[KEYDISP_DELAYEVENTS];
} KEYDISP;

static	KEYDISP		s_keydisp;

/**
 * @brief The table of the notes
 */
struct TagNotePattern
{
	UINT16 nPosX;			/*!< X-Coorinate */
	UINT8 nType;			/*!< type */
	const UINT8 *lpImage;	/*!< image */
};
typedef struct TagNotePattern NOTEPATTERN;		/*!< The define of the note's pattern */

/**
 * @brief const data
 */
struct KeyDispConstData
{
	UINT8 pal8[KEYDISP_PALS];			/*!< 8npp palettes */
	UINT16 pal16[2][KEYDISP_LEVEL];		/*!< 16bpp palettes */
	RGB32 pal32[2][KEYDISP_LEVEL];		/*!< 32bpp palettes */
	NOTEPATTERN pattern[128];			/*!< pattern */
};

/*! const data */
static struct KeyDispConstData s_constData;

#include "keydisp.res"


/* ---- event */

static void keyon(KEYDISP *keydisp, UINT ch, UINT8 note)
{
	UINT		i;
	KDCHANNEL	*kdch;

	note &= 0x7f;
	kdch = keydisp->ch + ch;
	for (i = 0; i < kdch->remain; i++)
	{
		if (kdch->k[i] == note)
		{
			/* ヒットした */
			for (; i < (kdch->remain - 1); i++)
			{
				kdch->k[i] = kdch->k[i + 1];
				kdch->r[i] = kdch->r[i + 1];
			}
			kdch->k[i] = note;
			kdch->r[i] = KEYDISP_LEVEL_MAX;
			kdch->flag |= 1;
			return;
		}
	}
	if (i < KEYDISP_NOTEMAX)
	{
		kdch->k[i] = note;
		kdch->r[i] = KEYDISP_LEVEL_MAX;
		kdch->flag |= 1;
		kdch->remain++;
	}
}

static void keyoff(KEYDISP *keydisp, UINT ch, UINT8 note)
{
	UINT		i;
	KDCHANNEL	*kdch;

	note &= 0x7f;
	kdch = keydisp->ch + ch;
	for (i = 0; i < kdch->remain; i++)
	{
		if (kdch->k[i] == note)
		{
			/* ヒットした */
			kdch->r[i] = (KEYDISP_LEVEL_MAX - 1);
			kdch->flag |= 1;
			break;
		}
	}
}

static void chkeyoff(KEYDISP *keydisp, UINT ch)
{
	UINT		i;
	KDCHANNEL	*kdch;

	kdch = keydisp->ch + ch;
	for (i = 0; i < kdch->remain; i++)
	{
		if (kdch->r[i] >= KEYDISP_LEVEL_MAX)
		{
			kdch->r[i] = (KEYDISP_LEVEL_MAX - 1);
			kdch->flag |= 1;
		}
	}
}

static void keyalloff(KEYDISP *keydisp)
{
	UINT i;

	for (i = 0; i < KEYDISP_CHMAX; i++)
	{
		chkeyoff(keydisp, i);
	}
}

static void keyallreload(KEYDISP *keydisp)
{
	UINT i;

	for (i = 0; i < KEYDISP_CHMAX; i++)
	{
		keydisp->ch[i].flag = 2;
	}
}

static void keyallclear(KEYDISP *keydisp)
{
	memset(keydisp->ch, 0, sizeof(keydisp->ch));
	keyallreload(keydisp);
}


/* ---- delay event */

static void ClearDelayList(KEYDISP *keydisp)
{
	keydisp->delay.warm = keydisp->delay.warmbase;
	keydisp->delay.pos = 0;
	keydisp->delay.rem = 0;
	memset(keydisp->delaye, 0, sizeof(keydisp->delaye));
	keyalloff(keydisp);
}

static void delayexecevent(KEYDISP *keydisp, UINT8 framepast)
{
	KDDELAYE	*ebase;
	UINT		pos;
	UINT		rem;
	KDDELAYE	*ev;

	ebase = keydisp->delaye;
	pos = keydisp->delay.pos;
	rem = keydisp->delay.rem;
	while ((keydisp->delay.warm) && (framepast))
	{
		keydisp->delay.warm--;
		framepast--;
		if (rem >= KEYDISP_DELAYEVENTS)
		{
			ev = ebase + pos;
			rem--;
			if (ev->ch == 0xff)
			{
				keydisp->delay.warm++;
			}
			else if (ev->key & 0x80)
			{
				keyon(keydisp, ev->ch, ev->key);
				rem--;
			}
			else
			{
				keyoff(keydisp, ev->ch, ev->key);
			}
			pos = (pos + 1) & (KEYDISP_DELAYEVENTS - 1);
		}
		ebase[(pos + rem) & (KEYDISP_DELAYEVENTS - 1)].ch = 0xff;
		rem++;
	}
	while (framepast)
	{
		framepast--;
		while (rem)
		{
			rem--;
			ev = ebase + pos;
			if (ev->ch == 0xff)
			{
				pos = (pos + 1) & (KEYDISP_DELAYEVENTS - 1);
				break;
			}
			if (ev->key & 0x80)
			{
				keyon(keydisp, ev->ch, ev->key);
			}
			else
			{
				keyoff(keydisp, ev->ch, ev->key);
			}
			pos = (pos + 1) & (KEYDISP_DELAYEVENTS - 1);
		}
		ebase[(pos + rem) & (KEYDISP_DELAYEVENTS - 1)].ch = 0xff;
		rem++;
	}
	keydisp->delay.pos = pos;
	keydisp->delay.rem = rem;
}

static void delaysetevent(KEYDISP *keydisp, REG8 ch, REG8 key)
{
	KDDELAYE	*e;

	e = keydisp->delaye;
	if (keydisp->delay.rem < KEYDISP_DELAYEVENTS)
	{
		e += (keydisp->delay.pos + keydisp->delay.rem) & (KEYDISP_DELAYEVENTS - 1);
		keydisp->delay.rem++;
		e->ch = ch;
		e->key = key;
	}
	else
	{
		e += keydisp->delay.pos;
		keydisp->delay.pos = (keydisp->delay.pos + 1) & (KEYDISP_DELAYEVENTS - 1);
		if (e->ch == 0xff)
		{
			keydisp->delay.warm++;
		}
		else if (e->key & 0x80)
		{
			keyon(keydisp, e->ch, e->key);
		}
		else
		{
			keyoff(keydisp, e->ch, e->key);
		}
		e->ch = ch;
		e->key = key;
	}
}


/* ---- OPM */

static const UINT8 s_opmkey[16] = { 0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9,10,11,12};

static UINT8 GetOpmNote(const OPMCTL *k, UINT16 wFNum)
{
	int nFraction;
	int nKey;

	nFraction = ((wFNum >> 10) & 0x3f) + k->wTone;
	nKey = ((((wFNum >> 4) & 7) + 2)) * 12 + s_opmkey[wFNum & 15] + ((nFraction + 32) >> 6);

	nKey = max(nKey, 0);
	nKey = min(nKey, 127);
	return nKey;
}

static void opmkeyoff(KEYDISP *keydisp, OPMCTL *k, UINT nChannel)
{
	delaysetevent(keydisp, (REG8)(k->cChannelNum + nChannel), k->ch[nChannel].cLastNote);
}

static void opmkeyon(KEYDISP *keydisp, OPMCTL *k, UINT nChannelNum)
{
	const UINT8 *pReg;

	opmkeyoff(keydisp, k, nChannelNum);

	pReg = k->pcRegister + 0x28 + nChannelNum;
	k->ch[nChannelNum].nFNumber = pReg[0] | (pReg[8] << 8);
	k->ch[nChannelNum].cLastNote = GetOpmNote(k, k->ch[nChannelNum].nFNumber);
	delaysetevent(keydisp, (REG8)(k->cChannelNum + nChannelNum), (REG8)(k->ch[nChannelNum].cLastNote | 0x80));
}

static void opmkeyreset(KEYDISP *keydisp)
{
	UINT i;

	for (i = 0; i < NELEMENTS(keydisp->opmctl); i++)
	{
		memset(keydisp->opmctl[i].ch, 0, sizeof(keydisp->opmctl[i].ch));
	}
}

void keydisp_opmkeyon(const UINT8 *pcRegister, REG8 cData)
{
	UINT i;
	OPMCTL *k;
	UINT nChannelNum;

	if (s_keydisp.mode != KEYDISP_MODEFM)
	{
		return;
	}

	for (i = 0; i < s_keydisp.opmmax; i++)
	{
		k = &s_keydisp.opmctl[i];
		if (k->pcRegister == pcRegister)
		{
			nChannelNum = cData & 7;
			cData &= 0x78;
			if ((nChannelNum < k->cFMChannels) && (k->ch[nChannelNum].cKeyOn != cData))
			{
				if (cData)
				{
					opmkeyon(&s_keydisp, k, nChannelNum);
				}
				else
				{
					opmkeyoff(&s_keydisp, k, nChannelNum);
				}
				k->ch[nChannelNum].cKeyOn = cData;
			}
			break;
		}
	}
}

static void opmkeysync(KEYDISP *keydisp)
{
	UINT i;
	OPMCTL *k;
	const UINT8 *pReg;
	UINT j;
	UINT8 n;
	UINT16 fnum;

	for (i = 0; i < keydisp->opmmax; i++)
	{
		k = &keydisp->opmctl[i];
		for (j = 0; j < k->cFMChannels; j++)
		{
			if (k->ch[j].cKeyOn)
			{
				pReg = k->pcRegister + 0x28 + j;
				fnum = pReg[0] | (pReg[8] << 8);
				if (k->ch[j].nFNumber != fnum)
				{
					k->ch[j].nFNumber = fnum;
					n = GetOpmNote(k, fnum);
					if (k->ch[j].cLastNote != n)
					{
						opmkeyoff(keydisp, k, j);
					}
					k->ch[j].cLastNote = n;
					delaysetevent(keydisp, (REG8)(k->cChannelNum + j), (REG8)(n | 0x80));
				}
			}
		}
	}
}


/* ---- PSG */

/**
 * Get pointer of controller
 * @param[in] pcRegister The instance of PSG
 * @return The pointer of controller
 */
static PSGCTL *GetController(KEYDISP *keydisp, const UINT8 *pcRegister)
{
	UINT i;
	PSGCTL *k;

	if (keydisp->mode != KEYDISP_MODEFM)
	{
		return NULL;
	}

	for (i = 0; i < keydisp->psgmax; i++)
	{
		k = &keydisp->psgctl[i];
		if (k->pcRegister == pcRegister)
		{
			return k;
		}
	}
	return NULL;
}

static UINT8 GetPSGNote(const PSGCTL *k, UINT16 nTone)
{
	UINT nOct;
	UINT nKey;

	nOct = 5;
	nTone &= 0xfff;

	while (nTone > k->wTone[0])
	{
		if (!nOct)
		{
			return 0;
		}
		nTone >>= 1;
		nOct--;
	}

	if (!nTone)
	{
		return 127;
	}

	while (nTone < k->wTone[12])
	{
		nTone <<= 1;
		nOct++;
	}
	for (nKey = 0; nTone < k->wTone[nKey + 1]; nKey++)
	{
	}
	nKey += nOct * 12;
	return (int)(min(nKey, 127));
}

static void psgmix(KEYDISP *keydisp, PSGCTL *k)
{
	const PSGREG *pReg;

	pReg = (const PSGREG *)k->pcRegister;
	if ((k->cLastMixer ^ pReg->mixer) & 7)
	{
		UINT8 i, bit, pos;
		k->cLastMixer = pReg->mixer;
		pos = k->cChannelNum;
		for (i = 0, bit = 1; i < 3; i++, pos++, bit <<= 1)
		{
			if (k->cPsgOn & bit)
			{
				k->cPsgOn ^= bit;
				delaysetevent(keydisp, pos, k->cLastNote[i]);
			}
			else if ((!(k->cLastMixer & bit)) && (pReg->vol[i] & 0x1f))
			{
				k->cPsgOn |= bit;
				k->nLastTone[i] = LOADINTELWORD(pReg->tune[i]) & 0xfff;
				k->cLastNote[i] = GetPSGNote(k, k->nLastTone[i]);
				delaysetevent(keydisp, pos, (UINT8)(k->cLastNote[i] | 0x80));
			}
		}
	}
}

static void psgvol(KEYDISP *keydisp, PSGCTL *k, UINT ch)
{
	const PSGREG *pReg;
	UINT8		bit;
	UINT8		pos;
	UINT16		tune;

	pReg = (const PSGREG *)k->pcRegister;
	bit = (1 << ch);
	pos = k->cChannelNum + ch;
	if (pReg->vol[ch] & 0x1f)
	{
		if (!((k->cLastMixer | k->cPsgOn) & bit))
		{
			k->cPsgOn |= bit;
			tune = LOADINTELWORD(pReg->tune[ch]);
			tune &= 0xfff;
			k->nLastTone[ch] = tune;
			k->cLastNote[ch] = GetPSGNote(k, tune);
			delaysetevent(keydisp, pos, (UINT8)(k->cLastNote[ch] | 0x80));
		}
	}
	else if (k->cPsgOn & bit)
	{
		k->cPsgOn ^= bit;
		delaysetevent(keydisp, pos, k->cLastNote[ch]);
	}
}

static void psgkeyreset(KEYDISP *keydisp)
{
	UINT i;

	for (i = 0; i < NELEMENTS(keydisp->psgctl); i++)
	{
		keydisp->psgctl[i].cPsgOn = 0;
	}
}

/**
 * Update keyboard
 * @param[in] pcRegister The instance
 * @param[in] nAddress The written register
 */
void keydisp_psg(const UINT8 *pcRegister, UINT nAddress)
{
	PSGCTL *k = GetController(&s_keydisp, pcRegister);
	if (k != NULL)
	{
		switch (nAddress)
		{
			case 7:
				psgmix(&s_keydisp, k);
				break;

			case 8:
			case 9:
			case 10:
				psgvol(&s_keydisp, k, nAddress - 8);
				break;
		}
	}
}

static void psgkeysync(KEYDISP *keydisp)
{
	UINT8		ch;
	const PSGREG *pReg;
	PSGCTL		*k;
	UINT8		bit;
	UINT8		i;
	UINT8		pos;
	UINT16		tune;
	UINT8		n;

	for (ch = 0, k = keydisp->psgctl; ch < keydisp->psgmax; ch++, k++)
	{
		pReg = (const PSGREG *)k->pcRegister;
		pos = k->cChannelNum;
		for (i = 0, bit = 1; i < 3; i++, pos++, bit <<= 1)
		{
			if (k->cPsgOn & bit)
			{
				tune = LOADINTELWORD(pReg->tune[i]);
				tune &= 0xfff;
				if (k->nLastTone[i] != tune)
				{
					k->nLastTone[i] = tune;
					n = GetPSGNote(k, tune);
					if (k->cLastNote[i] != n)
					{
						delaysetevent(keydisp, pos, k->cLastNote[i]);
						k->cLastNote[i] = n;
						delaysetevent(keydisp, pos, (UINT8)(n | 0x80));
					}
				}
			}
		}
	}
}



/* ---- BOARD change... */

/**
 * Reset
 */
void keydisp_reset(void)
{
	s_keydisp.keymax = 0;
	s_keydisp.opmmax = 0;
	s_keydisp.psgmax = 0;

	ClearDelayList(&s_keydisp);
	memset(&s_keydisp.opmctl, 0, sizeof(s_keydisp.opmctl));
	memset(&s_keydisp.psgctl, 0, sizeof(s_keydisp.psgctl));

	if (s_keydisp.mode == KEYDISP_MODEFM)
	{
		s_keydisp.dispflag |= KEYDISP_FLAGSIZING;
	}
}

/**
 * bind
 */
void keydisp_bindopm(const UINT8 *pcRegister, UINT nBaseClock)
{
	OPMCTL *k;

	if (((s_keydisp.keymax + 8) <= KEYDISP_CHMAX) && (s_keydisp.opmmax < NELEMENTS(s_keydisp.opmctl)))
	{
		k = &s_keydisp.opmctl[s_keydisp.opmmax];
		k->cChannelNum = s_keydisp.keymax;
		k->pcRegister = pcRegister;
		k->cFMChannels = 8;
		k->wTone = (SINT16)(log((double)nBaseClock / 3579545) / log(2.0) * 12.0 * 64.0);
		s_keydisp.opmmax++;
		s_keydisp.keymax += 8;
	}

	if (s_keydisp.mode == KEYDISP_MODEFM)
	{
		s_keydisp.dispflag |= KEYDISP_FLAGSIZING;
	}
}

/**
 * bind
 */
void keydisp_bindpsg(const UINT8 *pcRegister, UINT nBaseClock)
{
	PSGCTL *k;
	UINT i;

	if (((s_keydisp.keymax + 3) <= KEYDISP_CHMAX) && (s_keydisp.psgmax < NELEMENTS(s_keydisp.psgctl)))
	{
		k = &s_keydisp.psgctl[s_keydisp.psgmax];
		k->cChannelNum = s_keydisp.keymax;
		k->pcRegister = pcRegister;	
		for (i = 0; i < NELEMENTS(k->wTone); i++)
		{
			k->wTone[i] = (UINT16)((double)nBaseClock / 32.0 / (440.0 * pow(2.0, ((double)i - 9.5) / 12.0)));
		}
		s_keydisp.psgmax++;
		s_keydisp.keymax += 3;
	}

	if (s_keydisp.mode == KEYDISP_MODEFM)
	{
		s_keydisp.dispflag |= KEYDISP_FLAGSIZING;
	}
}



/* ---- MIDI */

void keydisp_midi(const UINT8 *cmd)
{
	if (s_keydisp.mode != KEYDISP_MODEMIDI)
	{
		return;
	}
	switch (cmd[0] & 0xf0)
	{
		case 0x80:
			keyoff(&s_keydisp, cmd[0] & 0x0f, cmd[1]);
			break;

		case 0x90:
			if (cmd[2] & 0x7f)
			{
				keyon(&s_keydisp, cmd[0] & 0x0f, cmd[1]);
			}
			else
			{
				keyoff(&s_keydisp, cmd[0] & 0x0f, cmd[1]);
			}
			break;

		case 0xb0:
			if ((cmd[1] == 0x78) || (cmd[1] == 0x79) || (cmd[1] == 0x7b))
			{
				chkeyoff(&s_keydisp, cmd[0] & 0x0f);
			}
			break;

		case 0xfe:
			keyalloff(&s_keydisp);
			break;
	}
}


/* ---- draw */

static UINT getdispkeys(const KEYDISP *keydisp)
{
	UINT keys;

	switch (keydisp->mode)
	{
		case KEYDISP_MODEFM:
			keys = keydisp->keymax;
			break;

		case KEYDISP_MODEMIDI:
			keys = 16;
			break;

		default:
			keys = 0;
			break;
	}
	return min(keys, KEYDISP_CHMAX);
}

static void clearrect(CMNVRAM *vram, int x, int y, int cx, int cy)
{
	CMNPAL col;

	switch (vram->bpp)
	{
#if defined(SUPPORT_8BPP)
		case 8:
			col.pal8 = s_constData.pal8[KEYDISP_PALBG];
			break;
#endif
#if defined(SUPPORT_16BPP)
		case 16:
			col.pal16 = s_constData.pal16[1][0];
			break;
#endif
#if defined(SUPPORT_24BPP) || defined(SUPPORT_32BPP)
		case 24:
		case 32:
			col.pal32 = s_constData.pal32[1][0];
			break;
#endif
		default:
			return;
	}
	cmndraw_fill(vram, x, y, cx, cy, col);
}

static void drawkeybg(CMNVRAM *vram)
{
	CMNPAL	bg;
	CMNPAL	fg;
	int		i;

	switch (vram->bpp)
	{
#if defined(SUPPORT_8BPP)
		case 8:
			bg.pal8 = s_constData.pal8[KEYDISP_PALBG];
			fg.pal8 = s_constData.pal8[KEYDISP_PALFG];
			break;
#endif
#if defined(SUPPORT_16BPP)
		case 16:
			bg.pal16 = s_constData.pal16[1][0];
			fg.pal16 = s_constData.pal16[0][0];
			break;
#endif
#if defined(SUPPORT_24BPP) || defined(SUPPORT_32BPP)
		case 24:
		case 32:
			bg.pal32 = s_constData.pal32[1][0];
			fg.pal32 = s_constData.pal32[0][0];
			break;
#endif
		default:
			return;
	}
	for (i = 0; i < 10; i++)
	{
		cmndraw_setpat(vram, keybrd1, i * KEYDISP_KEYCX, 0, bg, fg);
	}
	cmndraw_setpat(vram, keybrd2, 10 * KEYDISP_KEYCX, 0, bg, fg);
}

static BOOL draw1key(CMNVRAM *vram, KDCHANNEL *kdch, UINT n)
{
	const NOTEPATTERN *pPattern;
	UINT		pal;
	CMNPAL		fg;

	pPattern = s_constData.pattern + (kdch->k[n] & 0x7f);
	pal = kdch->r[n];
	switch (vram->bpp)
	{
#if defined(SUPPORT_8BPP)
		case 8:
			if (pal != KEYDISP_LEVEL_MAX)
			{
				fg.pal8 = s_constData.pal8[(pPattern->nType) ? KEYDISP_PALBG : KEYDISP_PALFG];
				cmndraw_setfg(vram, pPattern->lpImage, pPattern->nPosX, 0, fg);
				kdch->r[n] = 0;
				return TRUE;
			}
			fg.pal8 = s_constData.pal8[KEYDISP_PALHIT];
			break;
#endif
#if defined(SUPPORT_16BPP)
		case 16:
			fg.pal16 = s_constData.pal16[pPattern->nType][pal];
			break;
#endif
#if defined(SUPPORT_24BPP) || defined(SUPPORT_32BPP)
		case 24:
		case 32:
			fg.pal32 = s_constData.pal32[pPattern->nType][pal];
			break;
#endif
		default:
			return FALSE;
	}
	cmndraw_setfg(vram, pPattern->lpImage, pPattern->nPosX, 0, fg);
	return FALSE;
}

static BOOL draw1ch(CMNVRAM *vram, UINT8 framepast, KDCHANNEL *kdch)
{
	BOOL	draw;
	UINT	i;
	BOOL	coll;
	UINT8	nextf;
	UINT	j;

	draw = FALSE;
	if (kdch->flag & 2)
	{
		drawkeybg(vram);
		draw = TRUE;
	}
	if (kdch->flag)
	{
		coll = FALSE;
		nextf = 0;
		for (i = 0; i < kdch->remain; i++)
		{
			if ((kdch->r[i]) || (kdch->flag & 2))
			{
				if (kdch->r[i] < KEYDISP_LEVEL_MAX)
				{
					if (kdch->r[i] > framepast)
					{
						kdch->r[i] -= framepast;
						nextf = 1;
					}
					else
					{
						kdch->r[i] = 0;
						coll = TRUE;
					}
				}
				coll |= draw1key(vram, kdch, i);
				draw = TRUE;
			}
		}
		if (coll)
		{
			for (i = 0; i < kdch->remain; i++)
			{
				if (!kdch->r[i])
				{
					break;
				}
			}
			for (j = i; i < kdch->remain; i++)
			{
				if (kdch->r[i])
				{
					kdch->k[j] = kdch->k[i];
					kdch->r[j] = kdch->r[i];
					j++;
				}
			}
			kdch->remain = j;
		}
		kdch->flag = nextf;
	}
	return draw;
}


/* ---- */

void keydisp_initialize(void)
{
	int		r;
	UINT16	x;
	int		i;

	r = 0;
	x = 0;
	do
	{
		for (i = 0; i < 12 && r < 128; i++, r++)
		{
			s_constData.pattern[r].nPosX = s_notepattern[i].nPosX + x;
			s_constData.pattern[r].nType = s_notepattern[i].nType;
			s_constData.pattern[r].lpImage = s_notepattern[i].lpImage;
		}
		x += 28;
	} while (r < 128);
	keyallclear(&s_keydisp);
}

void keydisp_setpal(CMNPALFN *palfn)
{
	UINT i;
	RGB32 pal32[KEYDISP_PALS];

	if (palfn == NULL)
	{
		return;
	}
	if (palfn->get8)
	{
		for (i = 0; i < KEYDISP_PALS; i++)
		{
			s_constData.pal8[i] = (*palfn->get8)(palfn, i);
		}
	}
	if (palfn->get32)
	{
		for (i = 0; i < KEYDISP_PALS; i++)
		{
			pal32[i].d = (*palfn->get32)(palfn, i);
			cmndraw_makegrad(s_constData.pal32[0], KEYDISP_LEVEL, pal32[KEYDISP_PALFG], pal32[KEYDISP_PALHIT]);
			cmndraw_makegrad(s_constData.pal32[1], KEYDISP_LEVEL, pal32[KEYDISP_PALBG], pal32[KEYDISP_PALHIT]);
		}
		if (palfn->cnv16)
		{
			for (i = 0; i < KEYDISP_LEVEL; i++)
			{
				s_constData.pal16[0][i] = (*palfn->cnv16)(palfn, s_constData.pal32[0][i]);
				s_constData.pal16[1][i] = (*palfn->cnv16)(palfn, s_constData.pal32[1][i]);
			}
		}
	}
	s_keydisp.dispflag |= KEYDISP_FLAGREDRAW;
}

void keydisp_setmode(UINT8 mode)
{
	if (s_keydisp.mode != mode)
	{
		s_keydisp.mode = mode;
		s_keydisp.dispflag |= KEYDISP_FLAGREDRAW | KEYDISP_FLAGSIZING;
		keyallclear(&s_keydisp);
		if (mode == KEYDISP_MODEFM)
		{
			ClearDelayList(&s_keydisp);
			opmkeyreset(&s_keydisp);
			psgkeyreset(&s_keydisp);
		}
	}
	else
	{
		keyalloff(&s_keydisp);
	}
}

void keydisp_setdelay(UINT8 frames)
{
	s_keydisp.delay.warmbase = frames;
	ClearDelayList(&s_keydisp);
}

UINT8 keydisp_process(UINT8 framepast)
{
	UINT	keys;
	UINT	i;

	if (framepast)
	{
		if (s_keydisp.mode == KEYDISP_MODEFM)
		{
			opmkeysync(&s_keydisp);
			psgkeysync(&s_keydisp);
			delayexecevent(&s_keydisp, framepast);
		}
		s_keydisp.framepast += framepast;
	}

	keys = getdispkeys(&s_keydisp);
	for (i = 0; i < keys; i++)
	{
		if (s_keydisp.ch[i].flag)
		{
			s_keydisp.dispflag |= KEYDISP_FLAGDRAW;
			break;
		}
	}
	return s_keydisp.dispflag;
}

void keydisp_getsize(int *width, int *height)
{
	if (width)
	{
		*width = KEYDISP_WIDTH;
	}
	if (height)
	{
		*height = (getdispkeys(&s_keydisp) * KEYDISP_KEYCY) + 1;
	}
	s_keydisp.dispflag &= ~KEYDISP_FLAGSIZING;
}

BOOL keydisp_paint(CMNVRAM *vram, BOOL redraw)
{
	BOOL		draw;
	UINT		keys;
	UINT		i;
	KDCHANNEL	*p;

	draw = FALSE;
	if ((vram == NULL) || (vram->width < KEYDISP_WIDTH) || (vram->height <= 0))
	{
		goto kdpnt_exit;
	}
	if (s_keydisp.dispflag & KEYDISP_FLAGREDRAW)
	{
		redraw = TRUE;
	}
	if (redraw)
	{
		keyallreload(&s_keydisp);
		clearrect(vram, 0, 0, KEYDISP_WIDTH, 1);
		clearrect(vram, 0, 0, 1, vram->height);
		draw = TRUE;
	}
	vram->ptr += vram->xalign + vram->yalign;
	keys = (vram->height - 1) / KEYDISP_KEYCY;
	keys = min(keys, getdispkeys(&s_keydisp));
	for (i = 0, p = s_keydisp.ch; i < keys; i++, p++)
	{
		draw |= draw1ch(vram, s_keydisp.framepast, p);
		vram->ptr += KEYDISP_KEYCY * vram->yalign;
	}
	s_keydisp.dispflag &= ~(KEYDISP_FLAGDRAW | KEYDISP_FLAGREDRAW);
	s_keydisp.framepast = 0;

kdpnt_exit:
	return draw;
}
#endif
