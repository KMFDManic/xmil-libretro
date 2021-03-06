#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"


#if defined(ADV_SIMULATE)
	IOCORE		iocore;
#endif
	CGROM		cgrom;		//	x1k;
	CMT			cmt;
#if defined(ADV_SIMULATE)
	CRTC		crtc;
#endif
	CTC			ctc;
#if defined(ADV_SIMULATE)
	DMAC		dma;
#endif
	FDC			fdc;
#if defined(ADV_SIMULATE)
	PCG			pcg;
#endif
#if defined(ADV_SIMULATE)
	PPI			ppi;
#endif
	SIO			sio;
//	SNDBOARD	sndboard;
#if defined(ADV_SIMULATE)
	SUBCPU		subcpu;
#endif


// ----

#if defined(ADV_SIMULATE)
static void IOOUTCALL dummy_out(UINT port, REG8 dat) {

	(void)port;
	(void)dat;
}

static REG8 IOINPCALL dummy_inp(UINT port) {

	(void)port;
	return(0xff);
}
#else
void IOOUTCALL dummy_out(UINT port, REG8 dat);
REG8 IOINPCALL dummy_inp(UINT port);
#endif


// ----

static void IOOUTCALL port1fxx_o(UINT port, REG8 dat) {

	REG8	lsb;
	REG8	msb6;

	lsb = (UINT8)port;
	if (lsb == 0xd0) {
		scrn_o(port, dat);
		return;
	}
	if (lsb < 0x80) {
		return;
	}
	msb6 = lsb & (~3);
	if (lsb < 0x90) {
		dmac_o(port, dat);
		return;
	}
	if (msb6 == 0x90) {
		sio_o(port, dat);
		return;
	}
	if ((msb6 == 0xa0) || (msb6 == 0xa8)) {
		ctc_o(port, dat);
		return;
	}
	if (lsb == 0xe0) {
		blackctrl_o(port, dat);
		return;
	}
#if defined(SUPPORT_TURBOZ)
	if (pccore.ROM_TYPE >= 3) {
		if (lsb == 0xb0) {
			extpal_o(port, dat);
			return;
		}
		if (lsb == 0xc0) {
			exttextdisp_o(port, dat);
			return;
		}
		if (lsb == 0xc5) {
			extgrphpal_o(port, dat);
			return;
		}
		if ((lsb >= 0xb9) && (lsb < 0xc0)) {
			exttextpal_o(port, dat);
			return;
		}
	}
#endif
}

static REG8 IOINPCALL port1fxx_i(UINT port) {

	REG8	lsb;
	REG8	msb6;

	lsb = (UINT8)port;
	if (lsb < 0x80) {
		return(0xff);
	}
	msb6 = lsb & (~3);
	if (lsb < 0x90) {
		return(dmac_i(port));
	}
	if (msb6 == 0x90) {
		return(sio_i(port));
	}
	if ((msb6 == 0xa0) || (msb6 == 0xa8)) {
		return(ctc_i(port));
	}
	if (lsb == 0xd0) {
		return(scrn_i(port));
	}
	if (lsb >= 0xf0) {
		return(dipsw_i(port));
	}
#if defined(SUPPORT_TURBOZ)
	if (pccore.ROM_TYPE >= 3) {
		if (lsb == 0xb0) {
			return(extpal_i(port));
		}
		if (lsb == 0xc0) {
			return(exttextdisp_i(port));
		}
		if (lsb == 0xc5) {
			return(extgrphpal_i(port));
		}
		if ((lsb >= 0xb9) && (lsb < 0xc0)) {
			return(exttextpal_i(port));
		}
		if (lsb == 0xe0) {
			return(blackctrl_i(port));
		}
	}
#endif
	return(0xff);
}


// ----

static const IOINP definp[0x20] = {
			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp,

			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp,
			cgrom_i,			fdc_i,

			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp,
			pcg_i,				pcg_i,
			pcg_i,				pcg_i,

			dummy_inp,			subcpu_i,
			ppi_i,				sndboard_psgsta,
			dummy_inp,			dummy_inp,
			dummy_inp,			dummy_inp};

static const IOOUT defout[0x20] = {
			dummy_out,			dummy_out,
			dummy_out,			dummy_out,
			dummy_out,			dummy_out,
			dummy_out,			dummy_out,

			dummy_out,			dummy_out,
			dummy_out,			dummy_out,
			dummy_out,			dummy_out,
			cgrom_o,			fdc_o,

			palette_o,			palette_o,
			palette_o,			ply_o,
			dummy_out,			pcg_o,
			pcg_o,				pcg_o,

			crtc_o,				subcpu_o,
			ppi_o,				sndboard_psgdat,
			sndboard_psgreg,	memio_rom,
			memio_ram,			dummy_out};


typedef void (*INITFN)(void);

static const INITFN initfn[] = {
			cgrom_reset,	cmt_reset,		crtc_reset,
			ctc_reset,		dmac_reset,		fdc_reset,
			memio_reset,	pcg_reset,		ppi_reset,
			sio_reset,		sndboard_reset,	subcpu_reset,
			vramio_reset};


// ----

void iocore_reset(void) {

	UINT	i;

	ZeroMemory(&iocore, sizeof(iocore));
	CopyMemory(iocore.f.inpfn, definp, sizeof(definp));
	CopyMemory(iocore.f.outfn, defout, sizeof(defout));
	for (i=0; i<0x10; i++) {
		iocore.f.inpfn[i+0x20] = tram_atr_i;
		iocore.f.inpfn[i+0x30] = tram_ank_i;
		iocore.f.outfn[i+0x20] = tram_atr_o;
		iocore.f.outfn[i+0x30] = tram_ank_o;
	}
	if (pccore.ROM_TYPE >= 2) {
#if defined(SUPPORT_BANKMEM)
		iocore.f.inpfn[0x0b] = memio_bank_i;
		iocore.f.outfn[0x0b] = memio_bank_o;
#endif
		iocore.f.inpfn[0x1f] = port1fxx_i;
		iocore.f.outfn[0x1f] = port1fxx_o;
		for (i=0; i<8; i++) {
			iocore.f.inpfn[i+0x38] = tram_knj_i;
			iocore.f.outfn[i+0x38] = tram_knj_o;
		}
	}
#if defined(SUPPORT_TURBOZ)
	if (pccore.ROM_TYPE >= 3) {
		iocore.f.inpfn[0x10] = palette_i;
		iocore.f.inpfn[0x11] = palette_i;
		iocore.f.inpfn[0x12] = palette_i;
		iocore.f.inpfn[0x13] = ply_i;
	}
#endif
#if defined(SUPPORT_TURBOZ) || defined(SUPPORT_OPM)
	if (pccore.SOUND_SW) {
		iocore.f.inpfn[0x07] = opm_i;
		iocore.f.outfn[0x07] = opm_o;
	}
#endif
	for (i=0; i<NELEMENTS(initfn); i++) {
		(initfn[i])();
	}
}


// ----

#if defined(ADV_SIMULATE)
void IOOUTCALL iocore_out(UINT port, REG8 dat) {

	UINT	msb;

	msb = port >> 8;
	if (iocore.s.mode) {
		gram2_o(port, dat);
	}
	else if (msb >= 0x40) {
		gram_o(port, dat);
	}
	else {
		(*iocore.f.outfn[msb])(port, dat);
	}
}

REG8 IOINPCALL iocore_inp(UINT port) {

	UINT	msb;

	msb = port >> 8;
	iocore.s.mode = 0;
	if (msb >= 0x40) {
		return(gram_i(port));
	}
	else {
		return((*iocore.f.inpfn[msb])(port));
	}
}
#endif

