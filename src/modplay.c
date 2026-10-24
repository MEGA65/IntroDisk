/*
	---------------------------------------------------------------------------------------

	On the C65/MEGA65 the CIA timer runs at:
        PAL:   7.09375 / 7 = 1.0133928571428571428571428571429 MHz = 1013393 Hz
        NTSC:  7.15909 / 7 = 1.0227271428571428571428571428571 MHz = 1022727 Hz

    To trigger an IRQ at the same vertical position on screen every frame I trial-and-errored the timerA values to:
        PAL:  19623 ($4ca7)
        NTSC: 16568 ($40b8)

	Using the trial-and-error values to calculate VBLANK frequency: (CIA timer frequency (Hz) / number of ticks):
		PAL:  1013393Hz / 19623 = 51.643115585937784378389790406304 Hz
        NTSC: 1022727Hz / 16568 = 61.729064634062219769607505001032 Hz

	Turning this around, to calculate the CIA TimerA values (CIA timer frequency (Hz) / vblank frequency (Hz)):
	    PAL:  1013393 Hz / 51.64 Hz = 19623 ($4ca7)
		NTSC: 1022727 Hz / 61.73 Hz = 16568 ($40b8)

	Mega65/Protracker CIA (Complex Interface Adapter) Timer Tempo Calculations for 125 BPM:
	---------------------------------------------------------------------------------------
	
	AMIGA PAL:   Fcolor                                = 4.43361825 MHz (PAL color carrier frequency)
                 CPU Clock   = Fcolor * 1.6            = 7.0937892  MHz
                 CIA Clock   = Cpu Clock / 10          = 7.0937892 / 10 = 709.37892 kHz
                 50 Hz Timer = 1000 * (CIA Clock / 50) = 1000 * (709.37892 / 50) = 14187.5784 ticks
                 TEMPO NUM.  = 50 Hz Timer * 125       = 1773447

    AMIGA NTSC:  CPU Clock   =                         = 7.1590905 MHz
                 CIA Clock   = Cpu Clock / 10          = 7.1590905 / 10 = 715.90905 kHz
                 50 Hz Timer = 1000 * (CIA Clock / 50) = 1000 * (715.90905 / 50) = 14318.181 ticks
				 TEMPO NUM.  = 50 Hz Timer * 125       = 1789773

	MEGA65 PAL:  CIA Clock   =                         = 984.375 kHz
                 50 Hz Timer = 1000 * (CIA Clock / 50) = 1000 * (984.375 / 50) = 19687.5 ticks ~= 20268 ($4CE7)
				 TEMPO NUM.  = 50 Hz Timer * 125       = 2460875

				 NOT CALCULATED CORRECTLY YET. WAITING FOR NEW CORE:
	MEGA65 NTSC: CIA Clock   =                         = 1022.727 kHz
                 50 Hz Timer = 1000 * (CIA Clock / 50) = 1000 * (1022.727 / 50) = 20454.54 ticks ~= 20455 ($4FE7)
				 TEMPO NUM.  = 50 Hz Timer * 125       = 2556818

	To calculate tempo we use the formula: TimerValue = TEMPO NUM / Tempo = 2533483 / Tempo
	Tempo 125 will give a normal 50 Hz timer (VBlank).

	---------------------------------------------------------------------------------------
*/

// TODO:

// set song loop point correctly
// FINETUNE!!!
// reset function

#include <stdint.h>
#include "macros.h"
#include "registers.h"
#include "dma.h"
#include "modplay.h"
#include "audio.h"

extern void mp_calc_portdiffabs();

// #pragma clang section text="code" rodata="cdata" data="data" bss="zdata"

dma_copyjob mp_dmacopyjob;
//dma_copyjob mp_dmafilljob;

#define MP_MAX_INSTRUMENTS					32
#define MP_NUMRASTERS						(uint32_t)(2 * 312)	// PAL 0-311, NTSC 0-262
#define MP_RASTERS_PER_SECOND				(uint32_t)(MP_NUMRASTERS * 50)
#define MP_RASTERS_PER_MINUTE				(uint32_t)(MP_RASTERS_PER_SECOND * 60)
#define MP_NUM_SIGS							4

#define MP_SAMPLE_RATE_DIVISORLO			0x8B
#define MP_SAMPLE_RATE_DIVISORMID			0x67
#define MP_SAMPLE_RATE_DIVISORHI			0x16

// #define MP_RINGBUFFERSIZE					0x0800

#define MP_DEFAULT_TICKS_PER_FRAME			2452875
#define MP_DEFAULT_TICKS_PER_FRAME_XEMU		2496000

uint8_t mp_modsigs[MP_NUM_SIGS][4] =
{
	{ 0x4d, 0x2e, 0x4b, 0x2e },	// M.K.
	{ 0x4d, 0x21, 0x4b, 0x21 }, // M!K!
	{ 0x46, 0x4c, 0x54, 0x34 }, // FLT4
	{ 0x46, 0x4c, 0x54, 0x38 }  // FLT8
};

int16_t mp_waves[4*64] =
{
	// mp_sine[64]
	   0,   24,   49,   74,   97,  120,  141,  161,  180,  197,  212,  224,  235,  244,  250,  253,
	 254,  253,  250,  244,  235,  224,  212,  197,  180,  161,  141,  120,   97,   74,   49,   24,
	   0,  -24,  -49,  -74,  -97, -120, -141, -161, -180, -197, -212, -224, -235, -244, -250, -253,
	-254, -253, -250, -244, -235, -224, -212, -197, -180, -161, -141, -120,  -97,  -74,  -49,  -24,

	// mp_saw[64] // ramp down/saw down
	 255,  247,  239,  231,  223,  215,  207,  199,  191,  183,  175,  167,  159,  151,  143,  135,
	 127,  119,  111,  103,   95,   87,   79,   71,   63,   55,   47,   39,   31,   23,   15,    7,
	  -1,   -9,  -17,  -25,  -33,  -41,  -49,  -57,  -65,  -73,  -81,  -89,  -97, -105, -113, -121,
	-129, -137, -145, -153, -161, -169, -177, -185, -193, -201, -209, -217, -225, -233, -241, -249,

	// mp_square[64] // original source was between -256 and 255. AI says it's between -128 and 127
//	 255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,
//	 255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,  255,
//	-256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256,
//	-256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256,
 	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
 	 127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,

	/*
	// mp_random[64]
	  34,  201,   87,  155,   12,  244,   98,   63,  180,   29,  210,   77,  143,    6,  190,  121,
	  52,  168,  230,   15,   97,  205,   33,  149,   76,  222,   11,  134,  199,   58,  173,   90,
	 250,   39,  116,  181,   27,  240,   64,  138,  209,   83,    6,  156,  237,   44,  129,   72,
	 188,   21,  160,   93,  247,   36,  111,  195,   14,  173,   68,  222,   55,  144,   92,   31,
	 */
};

/*
uint32_t mp_ringbuffers[4] =
{
	0x0005e000,
	0x0005e800,
	0x0005f000,
	0x0005f800
};
*/

uint8_t mp_enabled_channels[4] = { 1, 1, 1, 1 };

// GLOBAL DATA ----------------------------------------------------------

uint8_t			mp_realhw;

uint8_t			mp_note_byte0						= 0;
uint8_t			mp_note_byte1						= 0;
uint8_t			mp_note_byte2						= 0;
uint8_t			mp_note_byte3						= 0;

uint8_t			mp_register_channel_offset			= 0;
int8_t			mp_period_base						= 0;

uint8_t			mp_triggersample					= 0;
uint8_t			mp_finetune							= 0;
uint8_t			mp_curchansamp						= 0;
uint32_t		mp_finetunemult						= 0;

int8_t			mp_tempfinetune						= 0;
uint8_t			mp_tempsamp							= 0;
uint8_t			mp_tempeffect						= 0;
uint8_t			mp_tempeffectdata					= 0;
uint8_t			mp_tempeffectdatalonyb				= 0;
uint8_t			mp_tempeffectdatahinyb				= 0;
uint16_t		mp_tempperiod						= 0;
uint32_t		mp_tempmultresult					= 0;
uint16_t		mp_tempfinetuneperiod				= 0;

uint8_t			mp_curchan							= 0;	// set this to 0,1,2,3 before calling mp_preprocesseffects or mp_processnote
uint16_t		mp_curperiod						= 0;	// set this before calling mp_findperiod

uint8_t			mp_playing;

uint8_t			mp_done						= 1;
uint8_t			mp_patternset;
uint8_t			mp_row;
uint8_t			mp_currow;
uint8_t			mp_pattern;
uint8_t			mp_delcount;
uint8_t			mp_globaltick;
uint8_t			mp_delset;
uint8_t			mp_inrepeat;
uint8_t			mp_addflag;
uint8_t			mp_arpeggiocounter;

uint8_t			mp_nextspeed;
uint8_t			mp_nexttempo;
//uint8_t		mp_nextticktime;

uint8_t			mp_loop						= 1;
uint8_t			mp_bpm						= 125;
uint8_t			mp_curpattern;
uint8_t			mp_song_loop_point;			// unused

uint8_t			freqlo;
uint8_t			freqhi;
uint8_t			sample_address0;
uint8_t			sample_address1;
uint8_t			sample_address2;
uint32_t		sample_adr;
uint32_t		sample_end_addr;

int16_t			portdiff;
uint16_t		portdiffabs;
uint16_t		portstep;

// VARIABLES FOR ATTIC->FAST DMA COPIES AND TIMING ----------------------

/*
int16_t			ringdiff;
int32_t			audiodata_left;
uint16_t		ringbufferpos;
uint16_t		samples_left;
uint16_t		ringdata_left;
uint16_t		ticks_prev_frame;
uint16_t		ticks_next_frame;
uint32_t		virtualaudiopos;
uint32_t		virtualaudioendaddress;
uint32_t		numsamples_next_frame;
uint32_t		numsamples_prev_frame;
*/

// MOD DATA FOR 1 MOD ---------------------------------------------------

uint8_t			mod_sigsize;										// size of signature (0 or 4)
uint8_t			mod_numinstruments;
uint8_t			mod_numpatterns;
uint8_t			mod_songlength;
uint8_t			mod_speed;											// SAME AS TICKS PER ROW
uint8_t			mod_currowdata						[16];			// data for current pattern
uint8_t			mod_tmpbuf							[23];
uint8_t			mod_patternlist						[128];
//uint8_t		mod_ticktime;
uint16_t		mod_tempo;											// SAME AS BPM
uint16_t		mod_currowdata_ptr;
uint32_t		mod_attic_addr;
uint32_t		mod_sample_offset;
uint32_t		mod_patternlist_offset;
uint32_t		mod_patterns_offset;
uint32_t		mod_patterns_data;
uint32_t		mod_patternlist_data;

// SAMPLE DATA FOR ALL INSTRUMENTS --------------------------------------

int8_t			sample_finetune						[MP_MAX_INSTRUMENTS];
uint8_t			sample_vol							[MP_MAX_INSTRUMENTS];
uint32_t		sample_lengths						[MP_MAX_INSTRUMENTS];
uint32_t		sample_repeatpoint					[MP_MAX_INSTRUMENTS];
uint32_t		sample_repeatlength					[MP_MAX_INSTRUMENTS];
uint32_t		sample_addr							[MP_MAX_INSTRUMENTS];

// CHANNEL DATA FOR ALL 4 CHANNELS --------------------------------------

int8_t			channel_volume						[4];
int8_t			channel_tempvolume					[4];
int8_t			channel_looppoint					[4];
int8_t			channel_loopcount					[4];
int8_t			channel_retrig						[4];			// seems to be always 0 and never set again
uint8_t			channel_sample						[4];
uint8_t			channel_repeat						[4];			// was bool, now 0-1 unsigned char
uint8_t			channel_stop						[4];			// was bool, now 0-1 unsigned char
uint8_t			channel_deltick						[4];
uint8_t			channel_portstep					[4];
uint8_t			channel_cut							[4];
uint8_t			channel_vibspeed					[4];
uint8_t			channel_vibwave						[4];
uint8_t			channel_vibpos						[4];
uint8_t			channel_vibdepth					[4];
uint8_t			channel_tremspeed					[4];
uint8_t			channel_tremwave					[4];
uint8_t			channel_trempos						[4];
uint8_t			channel_tremdepth					[4];
uint8_t			channel_glissctrl					[4];
uint16_t		channel_period						[4];
uint16_t		channel_arp							[4][3];
uint16_t		channel_portdest					[4];
uint16_t		channel_tempperiod					[4];
uint16_t		channel_offset						[4];
uint16_t		channel_offsetmem					[4];
uint16_t		channel_curringbufferpos			[4];
uint16_t		channel_prevringbufferpos			[4];
uint32_t		channel_atticsampleaddress			[4];
uint32_t		channel_atticsampleendaddress		[4];
uint32_t		channel_index						[4];			// used in 'retrigger note + x vblanks (ticks)', depends on offset

uint8_t			mp_emptysample						= 0;

uint16_t mp_periods[36+16] =
{
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,

	// extra data for arpeggios
	0,
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381
};

uint32_t mp_finetunes[16] =
{
	// [ This should be 2^(finetune/(12*8)). And 2^(1/96) is 1.007246412 on
    // my calculator...  (12 notes per octave and 1/8 of this)  -Lars Hamre ]

	//              1.007246412^(-finetune)
	69448, // -8    1.059732
	68944, // -7    1.052031
	68448, // -6    1.044399
	67949, // -5    1.036835
	67457, // -4    1.029338
	66972, // -3    1.021908
	66488, // -2    1.014544
	66011, // -1    1.007246
	65536, //  0    1.000000		// 2 ^ 0 = 1
	65065, //  1    0.992802		// 2 ^ (1/(12*8)) = 
	64595, //  2    0.985650
	64130, //  3    0.978553
	63670, //  4    0.971511
	63212, //  5    0.964524
	62755, //  6    0.957590
	62307, //  7    0.950710
};

// ------------------------------------------------------------------------------------

void modplay_reset()
{
	uint16_t i;
	uint8_t a;

	mp_row				= 0;
	mp_currow			= 0;
	mp_pattern			= 0;
	mp_curpattern		= 0;
	mp_delcount			= 0;
	mp_globaltick		= 0;
	mp_delset			= 0;
	mp_inrepeat			= 0;
	mp_addflag			= 0;
	mp_arpeggiocounter	= 0;
	mp_patternset		= 0;

	mod_speed			= 6;
	mp_nextspeed		= 6;
	mod_tempo			= 125;
	mp_nexttempo		= 125;
	//mod_ticktime		= 0; // 0.02;
	//mp_nextticktime	= 0; // 0.02;

	/*
	if(mp_realhw)
		ticks_next_frame	= MP_DEFAULT_TICKS_PER_FRAME / mod_tempo;
	else
		ticks_next_frame	= MP_DEFAULT_TICKS_PER_FRAME_XEMU / mod_tempo;	
	*/

	mp_done				= 0;

	for(i = 0; i < 4; i++)
	{
		channel_sample					[i] = 255; // no valid sample
		channel_volume					[i] = 0;
		channel_tempvolume				[i] = 0;
		channel_repeat					[i] = 0;
		channel_index					[i] = 0;
		channel_stop					[i] = 1;
		channel_deltick					[i] = 0;
		channel_period					[i] = 0;
		channel_portdest				[i] = 0;
		channel_tempperiod				[i] = 0;
		channel_portstep				[i] = 0;
		channel_cut						[i] = 0;
		channel_retrig					[i] = 0;
		channel_vibspeed				[i] = 0;
		channel_vibpos					[i] = 0;
		channel_vibdepth				[i] = 0;
		channel_tremspeed				[i] = 0;
		channel_tremwave				[i] = 0;
		channel_trempos					[i] = 0;
		channel_tremdepth				[i] = 0;
		channel_glissctrl				[i] = 0;
		channel_looppoint				[i] = 0;
		channel_loopcount				[i] = -1;
		channel_offset					[i] = 0;
		channel_offsetmem				[i] = 0;
		channel_curringbufferpos		[i] = 0;
		channel_prevringbufferpos		[i] = 0;
		channel_atticsampleaddress		[i] = 0;
		channel_atticsampleendaddress	[i] = 0;
		channel_vibwave					[i] = 0;

		mp_register_channel_offset = i << 4;

/*
		sample_adr = mp_ringbuffers[i];
		sample_address0 = (sample_adr >>  0) & 0xff;
		sample_address1 = (sample_adr >>  8) & 0xff;
		sample_address2 = (sample_adr >> 16) & 0xff;
		poke(0xd72a + mp_register_channel_offset, sample_address0);											// Set sample start address (LSB, MSB, BNK) to start of ringbuffer
		poke(0xd72b + mp_register_channel_offset, sample_address1);
		poke(0xd72c + mp_register_channel_offset, sample_address2);
		poke(0xd721 + mp_register_channel_offset, sample_address0);											// set repeat point (LSB, MSB, BNK) (alternative start point) to start of ringbuffer
		poke(0xd722 + mp_register_channel_offset, sample_address1);
		poke(0xd723 + mp_register_channel_offset, sample_address2);
		sample_adr = mp_ringbuffers[i] + MP_RINGBUFFERSIZE;								// set loop point (LSB, MSB, no BNK here) to end of ringbuffer.
		sample_address0 = (sample_adr >>  0) & 0xff;
		sample_address1 = (sample_adr >>  8) & 0xff;
		poke(0xd727 + mp_register_channel_offset, sample_address0);
		poke(0xd728 + mp_register_channel_offset, sample_address1);
*/				
	}

	// clear ringbuffers
/*
	for(a = 0; a < 4; a++)
		for(i=0; i<MP_RINGBUFFERSIZE; i++)
			lpoke(mp_ringbuffers[a]+i, 0);
*/

	// finally, enable audio DMA again
	AUDIO_DMA.AUDEN		= 0b10000000;
}

void modplay_enable()
{
	modplay_reset();
	mp_playing = 1;
}

void modplay_mute()
{
	// Mute all channels
	poke(0xd729, 0);
	poke(0xd739, 0);
	poke(0xd749, 0);
	poke(0xd759, 0);
	poke(0xd71c, 0);
	poke(0xd71d, 0);
	poke(0xd71e, 0);
	poke(0xd71f, 0);

	// disable audio dma
	AUDIO_DMA.AUDEN		= 0b00000000;

	// Stop all DMA audio first
	AUDIO_DMA.CHANNELS[0].CONTROL = 0;
	AUDIO_DMA.CHANNELS[1].CONTROL = 0;
	AUDIO_DMA.CHANNELS[2].CONTROL = 0;
	AUDIO_DMA.CHANNELS[3].CONTROL = 0;
}

void modplay_disable()
{
	mp_playing = 0;

	for(int i = 0; i < 4; i++)
	{
		mp_register_channel_offset = i << 4;
		poke(0xd720 + mp_register_channel_offset, 0x00);
	}
}

void modplay_toggleenable()
{
	if(mp_playing)
		modplay_disable();
	else
		modplay_enable();
}

void mp_dmacopy(uint32_t source_address, uint32_t destination_address, uint16_t count)
{
	mp_dmacopyjob.count					= count;
	mp_dmacopyjob.source_addr			= (source_address           ) & 0xffff;
	mp_dmacopyjob.source_bank_and_flags	= (source_address      >> 16) & 0x7f;
	mp_dmacopyjob.dest_addr				= (destination_address      ) & 0xffff;
	mp_dmacopyjob.dest_bank_and_flags	= (destination_address >> 16) & 0x7f;

	DMA.EN018B				= 0x01;
	DMA.ADDRBANK			= 0x00;
	DMA.ADDRMSB				= (((uint16_t)&mp_dmacopyjob) >> 8);
	DMA.ETRIG				= (((uint16_t)&mp_dmacopyjob) & 0xff);
}

/*
void mp_dmafill(uint16_t fill_word, uint32_t destination_address, uint16_t count)
{
	mp_dmafilljob.count					= count;
	mp_dmafilljob.source_addr			= (fill_word                ) & 0xffff;
	mp_dmafilljob.source_bank_and_flags	= (((uint32_t)0)       >> 16) & 0x7f;
	mp_dmafilljob.dest_addr				= (destination_address      ) & 0xffff;
	mp_dmafilljob.dest_bank_and_flags	= (destination_address >> 16) & 0x7f;

	DMA.EN018B				= 0x01;
	DMA.ADDRBANK			= 0x00;
	DMA.ADDRMSB				= (((uint16_t)&mp_dmafilljob) >> 8);
	DMA.ETRIG				= (((uint16_t)&mp_dmafilljob) & 0xff);
}
*/

// ------------------------------------------------------------------------------------

void mp_findperiod()
{
	mp_period_base = -1;

	// this takes 6 steps at most to reach final period (36/2/2/2/2/2/2) and returns a number between 0 and 35
	if(mp_curperiod < 113 || mp_curperiod > 856)
		return;

	uint8_t upper = 35;
	uint8_t lower = 0;
	uint8_t mid;
	uint16_t period;

	while((upper - lower) > 1)
	{
		mid = (lower + upper) >> 1;
		period = mp_periods[mid];
		if(mp_curperiod < period)	lower = mid;
		else						upper = mid;
	}

	uint16_t lower_period = mp_periods[lower];
	uint16_t upper_period = mp_periods[upper];

	if((lower_period - mp_curperiod) <= (mp_curperiod - upper_period))	mp_period_base = lower;
	else																mp_period_base = upper;
}


// ------------------------------------------------------------------------------------

void mp_preprocesseffects()
{
	uint8_t x = (mp_curchan << 2) + 2;

	mp_note_byte2 = peek(mod_currowdata_ptr+x); x++;
	mp_note_byte3 = peek(mod_currowdata_ptr+x);

	if ((mp_note_byte2 & 0x0f) == 0x0f) // set speed / tempo
	{
		mp_tempeffectdata = mp_note_byte3;

		if(mp_tempeffectdata > 0x1f)
		{
			mod_tempo			= mp_tempeffectdata;
			mp_nexttempo		= mp_tempeffectdata;
			// mod_ticktime		= 0; // 1 / (0.4 * mp_tempeffectdata);
			// mp_nextticktime	= 0; // 1 / (0.4 * mp_tempeffectdata);			
		}
		else
		{
			mod_speed = mp_tempeffectdata;
			mp_nextspeed = mp_tempeffectdata;
		}
	}
}

// ------------------------------------------------------------------------------------

void mp_processnote()
{
	uint8_t x = (mp_curchan << 2);

	mp_note_byte0 = peek(mod_currowdata_ptr+x); x++;
	mp_note_byte1 = peek(mod_currowdata_ptr+x); x++;
	mp_note_byte2 = peek(mod_currowdata_ptr+x); x++;
	mp_note_byte3 = peek(mod_currowdata_ptr+x);

	mp_register_channel_offset = mp_curchan << 4;

	mp_tempsamp       = ((mp_note_byte0 & 0xf0) | ((mp_note_byte2 >> 4) & 0x0f));
	mp_tempperiod     = (((uint16_t)(mp_note_byte0 & 0x0f)) << 8) | (uint16_t)(mp_note_byte1);
	mp_tempeffectdata = mp_note_byte3;
	mp_tempeffect     = mp_note_byte2 & 0x0f;

	mp_tempeffectdatalonyb = (mp_tempeffectdata     ) & 0x0f;
	mp_tempeffectdatahinyb = (mp_tempeffectdata >> 4) & 0x0f;

	if(mp_globaltick == 0 && mp_tempeffect == 0x0e && mp_tempeffectdatahinyb == 0x0d) // EDx : delay note x vblanks
		channel_deltick[mp_curchan] = mp_tempeffectdatalonyb % mod_speed;

	mp_triggersample = 0; // NOT IN ORIGINAL SOURCE - FIND BETTER WAY OF HANDLING THIS!!!

	if(mp_globaltick == channel_deltick[mp_curchan]) // 0 if no note delay
	{
		if((mp_tempperiod || mp_tempsamp) && !mp_inrepeat)
		{
			if(mp_tempsamp) // there is a sample, but there might not be a note
			{
				channel_stop[mp_curchan] = 0;
				mp_tempsamp--;
				if(mp_tempeffect != 0x03 && mp_tempeffect != 0x05) // not [Tone Portamento] and not [ToneP + Volsl]
				{
					mp_triggersample = 1;
					channel_offset[mp_curchan] = 0;
				}
				channel_sample[mp_curchan] = mp_tempsamp;
				channel_volume[mp_curchan] = sample_vol[mp_tempsamp];
				channel_tempvolume[mp_curchan] = channel_volume[mp_curchan];
			}

			// in the original source, this used to be inside the 'if(mp_tempperiod)' case, but I've seen mods
			// where there was no note/period but the sample offset was being set, so I moved it out.
			if(mp_tempeffect == 0x09) // Set SampleOffset [Offs:$00-$FF]
			{
				if(mp_tempeffectdata) channel_offsetmem[mp_curchan] = (uint16_t)(mp_tempeffectdata * 0x100);
				channel_offset[mp_curchan] += channel_offsetmem[mp_curchan];
			}

			if(mp_tempperiod) // there is a note, but there might not be a sample
			{
				if(mp_tempeffect != 0x03 && mp_tempeffect != 0x05) // not [Tone Portamento] and not [ToneP + Volsl]
				{
					mp_triggersample = 1;

					channel_index     [mp_curchan] = channel_offset[mp_curchan];
					channel_period    [mp_curchan]	= mp_tempperiod;
					channel_tempperiod[mp_curchan]	= mp_tempperiod;
					channel_stop      [mp_curchan]	= 0;
					channel_repeat    [mp_curchan]	= 0;
					channel_vibpos    [mp_curchan]	= 0;
					channel_trempos   [mp_curchan]	= 0;
				}
				channel_portdest[mp_curchan] = mp_tempperiod;
			}
		}

		//poke(0xc000, mp_tempeffect);
		//while(1) poke(0xd200, peek(0xd200)+1);

		switch (mp_tempeffect)
		{
			case 0x00: // Normal play or Arpeggio
				if(mp_tempeffectdata)
				{
					channel_period[mp_curchan] = channel_portdest[mp_curchan];
					mp_curperiod = channel_period[mp_curchan];
					mp_findperiod(); // calculate mp_period_base. it will be 0-35 or -1 if invalid

					mp_arpeggiocounter = 0;

					if(mp_period_base < 0)
					{
						channel_arp[mp_curchan][0] = mp_curperiod;
						channel_arp[mp_curchan][1] = mp_curperiod;
						channel_arp[mp_curchan][2] = mp_curperiod;
					}
					else
					{
						channel_arp[mp_curchan][0] = mp_curperiod;
						channel_arp[mp_curchan][1] = mp_periods[mp_period_base + mp_tempeffectdatahinyb];
						channel_arp[mp_curchan][2] = mp_periods[mp_period_base + mp_tempeffectdatalonyb];
					}
				}

				break;
			
			case 0x03: // Tone Portamento
				if(mp_tempeffectdata) channel_portstep[mp_curchan] = mp_tempeffectdata;
				break;

			case 0x04: // vibrato
				if(mp_tempeffectdatalonyb) channel_vibdepth[mp_curchan] = mp_tempeffectdatalonyb;
				if(mp_tempeffectdatahinyb) channel_vibspeed[mp_curchan] = mp_tempeffectdatahinyb;
				break;

			case 0x07: // tremolo
				if(mp_tempeffectdata)
				{
					channel_tremdepth[mp_curchan] = mp_tempeffectdatalonyb;
					channel_tremspeed[mp_curchan] = mp_tempeffectdatahinyb;
				}
				break;

			case 0x0b: // position jump
				if(mp_currow == mp_row) mp_row = 0;
				mp_pattern = mp_tempeffectdata;
				mp_patternset = 1;
				break;

			case 0xc: // set volume
				channel_volume[mp_curchan] = mp_tempeffectdata;
				channel_tempvolume[mp_curchan] = channel_volume[mp_curchan];
				break;

			case 0x0d: // mp_row jump
				if(mp_delcount) break;
				if(!mp_patternset) mp_pattern++;
				if(mp_pattern >= mod_songlength)
				{
					if(!mp_loop) mp_done = 1;
					mp_pattern = 0;
				}
				mp_row = mp_tempeffectdatahinyb * 10 + mp_tempeffectdatalonyb;
				mp_patternset = 1;
				if(mp_addflag) mp_row++; // emulate protracker EEx + Dxx bug
				break;

			case 0x0e:
			{
				switch(mp_tempeffectdatahinyb)
				{
					case 0x01:
						channel_period[mp_curchan] -= mp_tempeffectdatalonyb;
						channel_tempperiod[mp_curchan] = channel_period[mp_curchan];
						break;

					case 0x02:
						channel_period[mp_curchan] += mp_tempeffectdatalonyb;
						channel_tempperiod[mp_curchan] = channel_period[mp_curchan];
						break;

					case 0x06: // jump to loop, play x times
						if(mp_tempeffectdatalonyb)
						{
							if(channel_loopcount[mp_curchan] == -1)
							{
								channel_loopcount[mp_curchan] = mp_tempeffectdatalonyb;
								mp_row = channel_looppoint[mp_curchan];
							}
							else if(channel_loopcount[mp_curchan])
							{
								mp_row = channel_looppoint[mp_curchan];
							}
							channel_loopcount[mp_curchan]--;
						}
						else channel_looppoint[mp_curchan] = mp_row;
						break;

					case 0x0a:
						channel_volume[mp_curchan] += mp_tempeffectdatalonyb;
						channel_tempvolume[mp_curchan] = channel_volume[mp_curchan];
						break;

					case 0x0b:
						channel_volume[mp_curchan] -= mp_tempeffectdatalonyb;
						channel_tempvolume[mp_curchan] = channel_volume[mp_curchan];
						break;

					case 0x0c:
						channel_cut[mp_curchan] = mp_tempeffectdatalonyb;
						break;

					// case 0xd0: // d0 is special and handled at the start of this function
					//	break;

					case 0x0e: // delay pattern x notes
						if(!mp_delset) mp_delcount = mp_tempeffectdatalonyb;
						mp_delset = 1;
						mp_addflag = 1; // emulate bug that causes protracker to cause Dxx to jump too far when used in conjunction with EEx
						break;

					case 0x0f:
						// c->funkspeed = funktable[effectdata & 0x0f];
						break;

					default:
						break;
				}
			}

			default:
				break;
		}

		if(channel_tempperiod[mp_curchan] == 0 || channel_sample[mp_curchan] == 255 || sample_lengths[channel_sample[mp_curchan]] == 0)
		{
			channel_stop[mp_curchan] = 1;
		}
	}
	else if(channel_deltick[mp_curchan] == 0)
	{
		switch (mp_tempeffect)
		{
			case 0x00: // normal / arpeggio
				if(mp_tempeffectdata) channel_tempperiod[mp_curchan] = channel_arp[mp_curchan][mp_arpeggiocounter];
				break;

			case 0x01: // Portamento up - slide up
				channel_period[mp_curchan] -= mp_tempeffectdata;
				channel_tempperiod[mp_curchan] = channel_period[mp_curchan];
				break;

			case 0x02: // Portamento down - slide down
				channel_period[mp_curchan] += mp_tempeffectdata;
				channel_tempperiod[mp_curchan] = channel_period[mp_curchan];
				break;

			case 0x05: // volume slide + tone portamento
				if(mp_tempeffectdatahinyb)	channel_volume[mp_curchan] += mp_tempeffectdatahinyb;
				else						channel_volume[mp_curchan] -= mp_tempeffectdata;
				channel_tempvolume[mp_curchan] = channel_volume[mp_curchan];
				// no break, exploit fallthrough

			case 0x03: // tone portamento
				portdiff = channel_portdest[mp_curchan] - channel_period[mp_curchan];
				mp_calc_portdiffabs();
				portstep = channel_portstep[mp_curchan];
				if(portdiffabs < portstep)	channel_period[mp_curchan] = channel_portdest[mp_curchan];
				else if(portdiff > 0)		channel_period[mp_curchan] += portstep;
				else						channel_period[mp_curchan] -= portstep;

				if(channel_glissctrl[mp_curchan])
				{
					mp_curperiod = channel_period[mp_curchan];
					mp_findperiod();
					channel_tempperiod[mp_curchan] = mp_periods[mp_period_base]; // quantize OUTPUT ONLY
				}
				else
				{
					channel_tempperiod[mp_curchan] = channel_period[mp_curchan];
				}

				break;

			case 0x06: // volume slide + vibrato
				if(mp_tempeffectdatahinyb)	channel_volume[mp_curchan] += mp_tempeffectdatahinyb;
				else						channel_volume[mp_curchan] -= mp_tempeffectdata;
				channel_tempvolume[mp_curchan] = channel_volume[mp_curchan];
				// no break, exploit fallthrough

			case 0x04: // vibrato
				channel_tempperiod[mp_curchan]  = channel_period[mp_curchan] + ((channel_vibdepth[mp_curchan] * mp_waves[((channel_vibwave[mp_curchan] & 3) << 6) + channel_vibpos[mp_curchan]]) >> 7);
				channel_vibpos[mp_curchan]     += channel_vibspeed[mp_curchan];
				channel_vibpos[mp_curchan]     &= 63;
				break;

			case 0x07: // tremolo
				channel_tempvolume[mp_curchan]  = channel_volume[mp_curchan] + ((channel_tremdepth[mp_curchan] * mp_waves[((channel_tremwave[mp_curchan] & 3) << 6) + channel_trempos[mp_curchan]]) >> 6);
				channel_trempos[mp_curchan]    += channel_tremspeed[mp_curchan];
				channel_trempos[mp_curchan]    &= 63;
				break;

			case 0x0a: // volume slide
				if(mp_tempeffectdatahinyb)		channel_volume[mp_curchan] += mp_tempeffectdatahinyb; // slide up
				else							channel_volume[mp_curchan] -= mp_tempeffectdata; // slide down
				channel_tempvolume[mp_curchan]  = channel_volume[mp_curchan];
				break;

			case 0x0e: // E commands
			{
				switch(mp_tempeffectdatahinyb)
				{
					// case 0x00: break; // set filter (0 on, 1 off)
					// case 0x03: break; // glissando control (0 off, 1 on, use with tone portamento)
					// case 0x08: break; // marked as *NOT USED* in docs
					// case 0x0d: break; // d0 (PatternBreak) is special and handled at the start of this function

					case 0x03: // glissando control (0 off, 1 on, use with tone portamento)
						channel_glissctrl[mp_curchan] = mp_tempeffectdatalonyb;
						break;

					case 0x04: // set vibrato waveform (0 sine, 1 ramp down, 2 square)
						channel_vibwave[mp_curchan] = mp_tempeffectdatalonyb;
						break;

					case 0x05: // set finetune
						mp_tempfinetune = mp_tempeffectdatalonyb;
						if(mp_tempfinetune > 0x07) mp_tempfinetune |= 0xf0;
						sample_finetune[channel_sample[mp_curchan]] = mp_tempfinetune;
						break;

					case 0x07: // set tremolo waveform (0 sine, 1 ramp down, 2 square)
						channel_tremwave[mp_curchan] = mp_tempeffectdatalonyb;
						break;

					case 0x09: // retrigger note + x vblanks (ticks)
						if((mp_tempeffectdatalonyb == 0) || (mp_globaltick % mp_tempeffectdatalonyb) == 0) channel_index[mp_curchan] = channel_offset[mp_curchan];
						break;

					case 0x0c: // cut from note + x vblanks
						if(mp_globaltick == mp_tempeffectdatalonyb) channel_volume[mp_curchan] = 0;
						break;
				}
				break;
			}

			case 0xf: // Speed / tempo
				if(mp_tempeffectdata == 0)
				{
					mp_done = 1;
					break;
				}
				if (mp_tempeffectdata < 0x20) // speed (00-1F) / ticks per row (normally 6)
				{
					mp_nextspeed = (mp_tempeffectdata & 0x1f);	// mod_speed is same as ticks per row
				}
				else // tempo or BPM (20-FF)
				{
					mp_nexttempo = mp_tempeffectdata;
					//mp_nextticktime = 0; // 1 / (0.4f * mp_tempeffectdata);
				}

				break;
		}
	}

	if     (channel_volume[mp_curchan] < 0)			channel_volume[mp_curchan] = 0;
	else if(channel_volume[mp_curchan] > 63)		channel_volume[mp_curchan] = 63;

	if     (channel_tempvolume[mp_curchan] < 0)		channel_tempvolume[mp_curchan] = 0;
	else if(channel_tempvolume[mp_curchan] > 63)	channel_tempvolume[mp_curchan] = 63;
	
	if     (channel_tempperiod[mp_curchan] > 856)	channel_tempperiod[mp_curchan] = 856;
	else if(channel_tempperiod[mp_curchan] < 113)	channel_tempperiod[mp_curchan] = 113;

	//poke(0xd020, 0x07+channel);

	// SET VOLUME
	if(channel_stop[mp_curchan] || mp_enabled_channels[mp_curchan] == 0)						// CHANNEL STOP MEANS STOP OUTPUTTING AUDIO
	{
		poke(0xd729 + mp_register_channel_offset,     0);										// CH0VOLUME
		poke(0xd71c + mp_curchan,                     0);										// CH0RVOL
	}
	else
	{
		poke(0xd729 + mp_register_channel_offset,  channel_tempvolume[mp_curchan] << 0);							// CH0VOLUME, max is $ff

		// FORCE TO 0 SO AUDIO MIXER HANDLES splitting of audio (100% to one channel, 70% to other channel)
		// poke(0xd71c + mp_curchan, channel_tempvolume[mp_curchan] >> 1);						// CH0RVOL
		poke(0xd71c + mp_curchan, 0);															// CH0RVOL

		mp_curchansamp = channel_sample[mp_curchan];

		// TRIGGER SAMPLE

		if(mp_globaltick == channel_deltick[mp_curchan] && mp_triggersample == 1 && !channel_stop[mp_curchan])
		{
			// numsamples_prev_frame = 0;

			if(channel_tempperiod[mp_curchan] == 0)	// it's possible for there to be a sample but no note (period = 0). don't process note further if so.
				return;								// NOT SURE THIS IS CORRECT! SHOULD PROBABLY STILL COPY DATA FROM ATTIC TO FAST RAM?

			if(mp_enabled_channels[mp_curchan] == 0)
				return;

			poke(0xd720 + mp_register_channel_offset, 0x00);														// Stop playback while loading new sample data

			sample_adr = sample_addr[mp_curchansamp] + channel_offset[mp_curchan];
			sample_address0 = (sample_adr >>  0) & 0xff;
			sample_address1 = (sample_adr >>  8) & 0xff;
			sample_address2 = (sample_adr >> 16) & 0xff;

			poke(0xd72a + mp_register_channel_offset, sample_address0);												// Load sample address into current addr to set start address for playback
			poke(0xd72b + mp_register_channel_offset, sample_address1);
			poke(0xd72c + mp_register_channel_offset, sample_address2);

			if(sample_repeatpoint[mp_curchansamp])
			{
				sample_adr = (uint32_t)sample_addr[mp_curchansamp] + sample_repeatpoint[mp_curchansamp];
				sample_address0 = (sample_adr >>  0) & 0xff;
				sample_address1 = (sample_adr >>  8) & 0xff;
				sample_address2 = (sample_adr >> 16) & 0xff;
				poke(0xd721 + mp_register_channel_offset, sample_address0);											// set repeat point for repeating sample
				poke(0xd722 + mp_register_channel_offset, sample_address1);
				poke(0xd723 + mp_register_channel_offset, sample_address2);

				sample_end_addr = sample_adr + sample_repeatlength[mp_curchansamp];									// Sample loop end address
				sample_address0 = (sample_end_addr >>  0) & 0xff;
				sample_address1 = (sample_end_addr >>  8) & 0xff;
				poke(0xd727 + mp_register_channel_offset, sample_address0);
				poke(0xd728 + mp_register_channel_offset, sample_address1);

				poke(0xd720 + mp_register_channel_offset, 0b11000010);												// Enable playback +   looping of channel 0, 8-bit, no unsigned samples
			}
			else
			{
				sample_end_addr = sample_addr[mp_curchansamp] + sample_lengths[mp_curchansamp];		// Sample end address
				sample_address0 = (sample_end_addr >>  0) & 0xff;
				sample_address1 = (sample_end_addr >>  8) & 0xff;
				poke(0xd727 + mp_register_channel_offset, sample_address0);
				poke(0xd728 + mp_register_channel_offset, sample_address1);

				poke(0xd720 + mp_register_channel_offset, 0b10000010);												// Enable playback + nolooping of channel 0, 8-bit, no unsigned samples
			}

			// poke(0xd711, 0b10010000);															// Enable audio dma, enable bypass of audio mixer
			poke(0xd711, 0b10000000);															// Enable audio dma, enable bypass of audio mixer
		}
	}

	// SET FREQUENCY

	MATH.MULTINA0 = 0xff;																// calculate frequency // freq = 0xFFFFL / period
	MATH.MULTINA1 = 0xff;
	MATH.MULTINA2 = 0;
	MATH.MULTINA3 = 0;

	mp_tempfinetuneperiod = channel_tempperiod[mp_curchan];

	/*
	mp_finetune = sample_finetune[mp_curchansamp];
	if(mp_finetune != 0)
	{
		// frequency(final) = frequency(base) * 2 ^ (finetune/96)

		mp_finetune += 8;
		mp_finetunemult = mp_finetunes[mp_finetune];
		mp_tempfinetuneperiod = (mp_finetunemult * mp_tempfinetuneperiod) >> 16;
	}
	*/

	MATH.MULTINB0 = (mp_tempfinetuneperiod >> 0) & 0xff;
	MATH.MULTINB1 = (mp_tempfinetuneperiod >> 8) & 0xff;
	MATH.MULTINB2 = 0;
	MATH.MULTINB3 = 0;

	__asm(																					// wait 20 cycles at most for DIV calculation to finish
		" lda 0xd020\n"																		// maybe not needed any more. add volatile if this is needed
		" sta 0xd020\n"
		//" lda 0xd020\n"
		//" sta 0xd020\n"
		//" sta 0xd020"
	);

	freqlo = MATH.DIVOUTFRACT0;
	freqhi = MATH.DIVOUTFRACT1;

	MATH.MULTINA0 = MP_SAMPLE_RATE_DIVISORLO;
	MATH.MULTINA1 = MP_SAMPLE_RATE_DIVISORMID;
	MATH.MULTINA2 = MP_SAMPLE_RATE_DIVISORHI;
	MATH.MULTINA3 = 0;

	MATH.MULTINB0 = freqlo;
	MATH.MULTINB1 = freqhi;
	MATH.MULTINB2 = 0;
	MATH.MULTINB3 = 0;

	poke(0xd724 + mp_register_channel_offset, MATH.MULTOUT2);								// Pick results from output / 2^16
	poke(0xd725 + mp_register_channel_offset, MATH.MULTOUT3);
	poke(0xd726 + mp_register_channel_offset, 0);

	if(mp_globaltick == mod_speed - 1)
	{
		channel_tempperiod[mp_curchan] = channel_period[mp_curchan];
		channel_deltick[mp_curchan] = 0;
	}

	//poke(0xd020, 0x0f);
}

// ------------------------------------------------------------------------------------

void modplay_play() // was steptick in MFoP.c
{
	if(!mp_playing || mp_done)
		return;

	// always enable audio again, because user could have gone into freezer, which turns off audio dma
	AUDIO_DMA.AUDEN		= 0b10000000;

	// setup some DMA stuff that stays the same during modplay_play
	DMA.ADDRBANK		= 0;
	DMA.ADDRMB			= 0;

	mp_arpeggiocounter++;
	if(mp_arpeggiocounter == 3)
		mp_arpeggiocounter = 0;

	if(mp_row == 64)
	{
		mp_row = 0;
		mp_pattern++;
	}

	if(mp_pattern >= mod_songlength)
	{
		if(mp_loop)
		{
			mp_done			=    0;
			mp_pattern		=    0;
			mp_row			=    0;
			mp_globaltick	=    0;
			mod_speed		=    6;
			mp_nextspeed	=    6;
			mod_tempo		=  125;
			mp_nexttempo	=  125;
			//mod_ticktime		=    0; // 0.02;
			//mp_nextticktime	=    0; // 0.02;
		}
		else
		{
			mp_done = 1;
			return;
		}
	}

	// poke(0xd020, 0x11);

	if(mp_globaltick == 0)
	{
		mp_patternset	= 0;
		mp_currow		= mp_row;
		mp_curpattern	= mp_pattern;

		mp_dmacopy(mod_patterns_data + ((uint32_t)(mod_patternlist[mp_curpattern]) << 10) + (mp_currow << 4), (uint32_t)mod_currowdata, 16);

		mp_curchan = 0; mp_preprocesseffects();
		mp_curchan = 1; mp_preprocesseffects();
		mp_curchan = 2; mp_preprocesseffects();
		mp_curchan = 3; mp_preprocesseffects();

		mod_speed = mp_nextspeed;
		mod_tempo = mp_nexttempo;
		//mod_ticktime = mp_nextticktime;
	}

	mp_curchan = 0; mp_processnote();
	mp_curchan = 1; mp_processnote();
	mp_curchan = 2; mp_processnote();
	mp_curchan = 3; mp_processnote();

	mp_globaltick++;
	if(mp_globaltick == mod_speed)
	{
		if(mp_delcount)
		{
			mp_inrepeat = 1;
			mp_delcount--;
		}
		else
		{
			mp_delset	= 0;
			mp_addflag	= 0;
			mp_inrepeat	= 0;

			if(mp_currow == mp_row && !mp_patternset)				// !MP_PATTERNSET TEST ONLY ADDED FOR PATTERN_SKANK MOD, NOT SURE IF CORRECT ALWAYS!!!
				mp_row++;
		}

		mp_globaltick = 0;
	}
}

// ------------------------------------------------------------------------------------

void modplay_initmod(uint32_t address)
{
	uint16_t i;
	uint8_t a;

	mp_dmacopy(address + 1080, (uint32_t)mod_tmpbuf, 4);							// Check if 15 or 31 instrument mode (M.K.)

	mod_sigsize = 0;
	mod_numinstruments = 15;

	mod_tmpbuf[4] = 0;
	for(i = 0; i < MP_NUM_SIGS; i++)
	{
		for(a = 0; a < 4; a++)
			if(mod_tmpbuf[a] != mp_modsigs[i][a])
				break;
		if(a == 4)
		{
			mod_sigsize = 4;
			mod_numinstruments = 31;
		}
	}

	mod_patternlist_offset = 20 + mod_numinstruments * 30 + 2;
	mod_patterns_offset = mod_sigsize + mod_patternlist_offset + 128;					// 600 for 15 instruments, 1084 for 31

	mod_patterns_data = address + mod_patterns_offset;
	mod_patternlist_data = address + mod_patternlist_offset;

	for(i = 0; i < mod_numinstruments; i++)
	{
		// 2 bytes - sample length in words. multiply by 2 for byte length
		// 1 byte  - lower 4 bits for finetune, upper 4 not used, set to 0
		// 1 byte  - volume for sample ($00-$40)
		// 2 bytes - repeat point in words
		// 2 bytes - repeat length in words
		mp_dmacopy(address + 20 + i * 30 + 22, (uint32_t)mod_tmpbuf, 8);			// Get instrument data
		sample_lengths      [i] = mod_tmpbuf[1] + (mod_tmpbuf[0] << 8);
		sample_lengths      [i] <<= 1;													// Redenominate instrument length into bytes

		// finetune is a signed-4bit-number. convert to signed-8-bit
		mp_tempfinetune = mod_tmpbuf[2] & 0x0f;
		if(mp_tempfinetune > 0x07) //0b00000111
			mp_tempfinetune |= 0xf0; // 0b11110000

		sample_finetune     [i] = mp_tempfinetune;
		sample_vol          [i] = mod_tmpbuf[3];										// Instrument volume
		sample_repeatpoint  [i] = (((uint32_t)mod_tmpbuf[4]) << 8) + mod_tmpbuf[5];		// Repeat start point and end point
		sample_repeatpoint  [i] <<= 1;
		sample_repeatlength [i] = (((uint32_t)mod_tmpbuf[6]) << 8) + mod_tmpbuf[7];
		sample_repeatlength [i] <<= 1;
	}

	mod_songlength = lpeek(address + 20 + mod_numinstruments * 30 + 0);
	mp_song_loop_point = lpeek(address + 20 + mod_numinstruments * 30 + 1);

	mod_numpatterns = 0;
	mp_dmacopy(mod_patternlist_data, (uint32_t)mod_patternlist, 128);
	for(i = 0; i < mod_songlength; i++)
	{
		if(mod_patternlist[i] > mod_numpatterns)
			mod_numpatterns = mod_patternlist[i];
	}
	
	/* DEBUG INFO FOR		Mod.Christmas Groove.Mod

	INSTRUMENTS:                      ABSSTART    START       END         LENGTH      REPTSTRT    FINT    VOLM
	01    [   i wish you all a  ·]    0x005C3C    0x000000    0x005094    0x005094    0x0033E4    0x00    0x40
	02    [ very merry christmas·]    0x00ACD2    0x005096    0x00AB0E    0x005A78    0x007EEE    0x00    0x40
	03    [ and a happy new year·]    0x01074E    0x00AB12    0x00E728    0x003C16    0x000000    0x00    0x20
	04    [however, concidering··]    0x014364    0x00E728    0x00E728    0x000000    0x000000    0x00    0x00
	05    [the fact that it's····]    0x014364    0x00E728    0x00E728    0x000000    0x000000    0x00    0x00
	06    [christmas, we should··]    0x014364    0x00E728    0x011ED0    0x0037A8    0x000000    0x00    0x40
	07    [think of those less···]    0x017B0C    0x011ED0    0x013A44    0x001B74    0x0133A0    0x00    0x40
	08    [fortunate than ·······]    0x019682    0x013A46    0x01AF72    0x00752C    0x016820    0x00    0x40
	09    [ourselves. just think·]    0x020BB2    0x01AF76    0x01AF76    0x000000    0x000000    0x00    0x00
	10    [of those poor people··]    0x020BB2    0x01AF76    0x01D67A    0x002704    0x000000    0x00    0x40
	11    [who have but an atari·]    0x0232B6    0x01D67A    0x01D67A    0x000000    0x000000    0x00    0x00
	12    [pc or mac! yes they···]    0x0232B6    0x01D67A    0x01D67A    0x000000    0x000000    0x00    0x00
	13    [are a sorry bunch!!···]    0x0232B6    0x01D67A    0x01FD66    0x0026EC    0x000000    0x00    0x40
	14    [now let's party!······]    0x0259A2    0x01FD66    0x02266E    0x002908    0x000000    0x00    0x40
	15    [       /paul draaisma·]    0x0282AA    0x02266E    0x0244DA    0x001E6C    0x000000    0x00    0x40
	16    [a.k.a. cortex  *<:-)··]    0x02A116    0x0244DA    0x0254FA    0x001020    0x000000    0x00    0x40
	17    [this song was c'posed·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	18    [in the early hours of·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	19    [the 13th of december··]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	20    [         1993        ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	21    [contact me for any····]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	22    [sick reason, as long ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	23    [as it is legal, on:···]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	24    [    paul draaisma    ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	25    [   magistratsv. 55   ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	26    [        z 121        ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	27    [     226 44  lund    ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	28    [        sweden       ·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	29    [ofcourse, you could···]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	30    [dial +46 (0)46 392864·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	31    [(c)1993 paul draaisma·]    0x02B136    0x0254FA    0x0254FA    0x000000    0x000000    0x00    0x00
	*/

	uint32_t mod_sample_offset = mod_patterns_offset + ((uint32_t)mod_numpatterns + 1) * 1024;

	// set sample addresses
	uint32_t mod_sample_data = address + mod_patterns_offset + ((uint32_t)mod_numpatterns + 1) * 1024;
	for(i = 0; i < MP_MAX_INSTRUMENTS; i++)
	{
		sample_addr[i] = mod_sample_data;
		mod_sample_data += sample_lengths[i];
	}

	modplay_reset();
}

// ------------------------------------------------------------------------------------

void modplay_init()
{
	uint16_t i;

	// turn off saturation
	AUDIO_DMA.DBGSAT	= 0b00000000;

	// set up dma values that don't change
	mp_dmacopyjob.command_lo		= 0b00000000; // copy, no chain
	mp_dmacopyjob.sourcemb_token	= 0x80;
	mp_dmacopyjob.destmb_token		= 0x81;
	mp_dmacopyjob.format			= 0x0b;
	mp_dmacopyjob.endtokenlist		= 0x00;
	mp_dmacopyjob.command_hi		= 0x00;
	mp_dmacopyjob.sourcemb			= 0x00; // version of modplay that doesn't do DMA copies from attic MB
	mp_dmacopyjob.destmb			= 0x00; // modplay only does DMA copies to fast MB

	/*
	mp_dmafilljob.command_lo		= 0b00000011; // fill, no chain
	mp_dmafilljob.sourcemb_token	= 0x80;
	mp_dmafilljob.destmb_token		= 0x81;
	mp_dmafilljob.format			= 0x0b;
	mp_dmafilljob.endtokenlist		= 0x00;
	mp_dmafilljob.command_hi		= 0x00;
	mp_dmafilljob.sourcemb			= 0x00; // source not used for fill
	mp_dmafilljob.destmb			= 0x00; // modplay only does DMA fills to fast MB
	*/

	audio_save_master_volumes();	// save user's volume settings before we fiddle

	mp_realhw = ((peek(0xd60f) >> 5) & 0x01);

	mod_currowdata_ptr = (uint16_t)&(mod_currowdata[0]);

	// audioxbar_setcoefficient(i, 0xff);

	// commented out, because I was inadvertently forcing the left and right side to the same values and making things sound mono instead of stereo
	/*
	for(i = 0; i < 256; i++)
	{
		// Select the coefficient
		poke(0xd6f4, i);

		// Now wait at least 16 cycles for it to settle
		poke(0xd020, peek(0xd020));
		poke(0xd020, peek(0xd020));

		// set value to 0xc0
		poke(0xd6f5, 0xc0);
	}
	*/

	poke(&audio_volume, 0x80);		// this should really be $40, but that's coming out way too muted?
																// GI: bring this initial volume down a notch to be at a similar level to past intro disks
	audio_applyvolume();

	modplay_disable();
	modplay_mute();
}

// ------------------------------------------------------------------------------------
