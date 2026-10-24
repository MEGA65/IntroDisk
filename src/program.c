#include <stdint.h>
#include "macros.h"
#include "registers.h"
#include "hyppo.h"
#include "constants.h"
#include "modplay.h"
#include "iffl.h"
#include "irqload.h"
#include "keyboard.h"
#include "dma.h"
#include "fontsys.h"
#include "dmajobs.h"
#include "program.h"
#include "audio.h"

typedef struct _category
{
	uint16_t cat_entry_offset;
	uint16_t name;
	uint8_t  parent_cat_idx;
} category;

typedef struct _catentry
{
	uint16_t title;
	uint16_t full;
	uint16_t desc;
	uint16_t author;
	uint16_t mount;
	uint8_t  dir_flag;
} catentry;

uint32_t			program_rowoffset			= 0;
uint16_t			program_numtxtentries		= 0;
uint8_t				program_keydowncount		= 0;
uint8_t				program_keydowndelay		= 0;
int16_t				program_selectedrow			= 0;

uint8_t				xemu_fudge = 8;

uint32_t			menubinaddr = 0x1f900;
uint8_t				program_numcategories;
uint8_t				program_numbasecategories;
uint8_t				program_numentries;

uint8_t				program_textbank;

__far uint8_t*		program_menubin_struct;
__far category*		program_categories;
__far catentry*		program_entries;
__far catentry*		program_current_entry;

uint8_t				current_cat_idx;
uint8_t				current_ent_idx;

uint8_t				c_textyposstart = 6 * 0x10;
uint8_t				c_textyposoffset = 0;

uint8_t				c_textypos;
int8_t				movedir = 0;
uint8_t				movespeed = 2;

uint8_t				program_state = 0; // 0 = intro screen, 1 = browsing menu.bin

uint8_t				program_urlsprsize = 32;
uint8_t				program_urlsprsize2 = 32;

uint8_t				program_category_indices[256];
uint8_t				program_category_selectedrows[257]; // 257 because program_setcategory uses current_cat_idx+1 and current_cat_idx can be 0xff -> 0x0100

uint8_t				program_deferredindex;
uint8_t				program_deferredrow;
uint8_t				program_deferredendrow;

uint8_t				sprwidth  = 64;
uint8_t				sprheight = 48;

uint8_t				program_showingqrcode = 0;
uint8_t				program_qrcodexposmax = 140;
uint8_t				program_qrcodexpos = 140;
uint8_t				program_qrcodexposmin = 96;

uint16_t			program_maintextxpos = 80;

uint8_t				program_maintextxscale = 0x78;
uint8_t				program_headertextxscale = 0x78;
uint8_t				program_footertextxscale = 0x78;

uint8_t				program_bounceframe;

uint16_t			program_categorytimer = 0;

uint8_t defaultromstring[]		= "MEGA65.ROM\x00";
uint8_t autobootstring[]		= "AUTOBOOT.C65\x00";
uint8_t mega65d81string[]		= "mega65.d81\x00";
uint8_t intro4d81string[]		= "INTRO5.D81\x00";

uint8_t introtext1[]			= "\x80 THE mega65 COMMUNITY PRESENTS\x00";
uint8_t introtext2[]			= "\x80 2026 - rom 920xxx - pal mode\x00";
uint8_t introtext3[]			= "\x85pRESS m TO toggle music. pRESS reset AT ANY TIME TO return to this menu\x00";

uint8_t headermaintext[]		= "\x80 cURRENTLY BROWSING:\x00";
uint8_t headermaintext2[]		= "\x81mAIN MENU\x00";
uint8_t headercategorytext[]	= "\x80 cURRENTLY BROWSING:\x00";
uint8_t headerentrytext[]		= "\x80 cURRENTLY VIEWING:\x00";

uint8_t footertext0[]			= "\x80PRESS\x82 return\x80 \x18\x19 TO\x82 continue\x00";
uint8_t footertext1[]			= "\x80uSE cursor keys \x15 \x16 TO scroll AND back \x17 or / \x1a TO go back\x00";
uint8_t footertext2[]			= "\x80pRESS return \x18\x19 TO select\x00";
uint8_t footertext3[]			= "\x80pRESS\x82 return\x80 \x18\x19 TO\x82 start program\x00";
uint8_t footertext4[]			= "\x80uSE cursor keys \x15 \x16 TO scroll AND 'i' TO GO TO intro disk selector\x00";

uint8_t loadingtext1[]			= "\x82 mount:\x00";
uint8_t loadingtext2[]			= "\x82 prg:\x00";
uint8_t loadingtext3[]			= "\x80 loading...\x00";

uint8_t loadingntsc[]			= "\x81 eNFORCING \x82ntsc\x81 MODE...\x00";
uint8_t loadingpal[]			= "\x81 eNFORCING \x83pal\x81 MODE...\x00";

uint8_t showing_credits = 0;

__far char *ptr;

extern uint8_t fl_iffl_debug;

// forward function declarations
void program_drawtextscreen();

#define NUM_SPECIAL_CATS 6
#define DEMO_CAT_IDX 1
#define MAX_BOUNCE_FRAMES 46

char str[128];

void program_setmaintextxpos(uint16_t xpos)
{
	poke(&maintextxposlo, (xpos >> 0) & 0xff);
	poke(&maintextxposhi, (xpos >> 8) & 0xff);
}

void program_settextbank(uint8_t bank)
{
	program_textbank = bank;
	poke(0x5e, bank);
}

void program_setcategorytextbank()
{
	program_showingqrcode = 0;
	program_settextbank(2);
	if((current_cat_idx >= program_numcategories-NUM_SPECIAL_CATS && current_cat_idx < program_numcategories)
			|| current_cat_idx == DEMO_CAT_IDX) // force 'Demo' category items into 2nd bank too (for more space)
		program_settextbank(5);
}

void program_checkdrawQR()
{
	// don't check for QR if we're rendering categories
	if(current_ent_idx == 0xff)
	{
		program_showingqrcode = 0;
		return;
	}

	// is there a URL at this line?
	uint8_t urlsprindex = peek(&fnts_lineurlstart + program_selectedrow);
	if(urlsprindex != 255)
	{
		program_showingqrcode = 1;

		program_urlsprsize = 4+(uint8_t)peek(&fnts_lineurlsize + program_selectedrow);
		program_urlsprsize2 = program_urlsprsize; // counts down in program_renderqrbackground

		program_qrcodexposmin = 96 - 2*program_urlsprsize;

		uint8_t spriteypos = 228 - 2*program_urlsprsize - palntscyoffset;

		poke(SPRITEPTRS+6, urlsprindex);
		poke(SPRITEPTRS+7, 0);

		VIC2.S0Y = spriteypos + 4;
		VIC2.S1Y = spriteypos + 4;
		VIC2.S2Y = spriteypos + 2*program_urlsprsize - 14 - 4; // 14 = anchor sprite size

		VIC2.S3Y = spriteypos;
		VIC2.S4Y = spriteypos;

		program_renderqrbackground();
	}
	else
	{
		program_showingqrcode = 0;
	}
}

void program_drawline(uint16_t entry, uint8_t color, uint8_t row, uint8_t column)
{
	fnts_row = row;
	fnts_column = column;

	poke(&fnts_bottomlineadd1 + 1, 0x80);
	poke(&fnts_bottomlineadd2 + 1, 0);

	poke(&fnts_curpal + 1, color);

	poke(0x5c, (entry >> 0) & 0xff);
	poke(0x5d, (entry >> 8) & 0xff);

	fontsys_asm_setupscreenpos();
	fontsys_asm_render();
}

void program_clearheader()
{
	dma_runjob((__far char *)&dma_clearheaderlinecolorram1);
	dma_runjob((__far char *)&dma_clearheaderlinecolorram2);
	dma_runjob((__far char *)&dma_clearheaderlinescreenram1);
	dma_runjob((__far char *)&dma_clearheaderlinescreenram2);
}

void program_clearfooters()
{
	dma_runjob((__far char *)&dma_clearfooterlinescolorram1);
	dma_runjob((__far char *)&dma_clearfooterlinescolorram2);
	dma_runjob((__far char *)&dma_clearfooterlinesscreenram1);
	dma_runjob((__far char *)&dma_clearfooterlinesscreenram2);
}


void program_drawintroheader()
{
	program_clearfooters();
	program_drawline((uint16_t)&footertext0, 0x00, 38, 27*2);
}

void program_drawintrofooter()
{
	program_clearfooters();
	program_drawline((uint16_t)&footertext0, 0x00, 38, 27*2);
}

void program_drawmaincategoryheader()
{
	poke(&program_bounceselectionline, 1);

	fontsys_map();
	program_clearheader();
	program_settextbank(0);
	program_drawline((uint16_t)&headermaintext, 0x00, 0, 0);
	program_drawline((uint16_t)&headermaintext2, 0x1f, 0, 2*21);
	program_setcategorytextbank();
	fontsys_unmap();
}

void program_drawmaincategoryfooter()
{
	fontsys_map();
	program_clearfooters();
	program_settextbank(0);
	program_drawline((uint16_t)&footertext4, 0x00, 38, 10*2);
	program_drawline((uint16_t)&footertext2, 0x00, 40, 26*2);
	program_setcategorytextbank();
	fontsys_unmap();
}

void program_gencategory_tree()
{
	uint8_t idx = 0;
	uint8_t str_idx = 0;
	uint8_t cat_idx = 0;
	uint8_t cat_tree[5] = { 0 };
	uint8_t tbank;

	idx = current_cat_idx;
	cat_tree[cat_idx] = current_cat_idx;
	cat_idx++;

	while ( (idx = program_categories[idx].parent_cat_idx) != 0xff)
	{
		cat_tree[cat_idx] = idx;
		cat_idx++;
	}

	for (idx = cat_idx-1; idx != 0xff; idx--)	// I wanted to do idx >= 0, but it's a uint_8...
	{
		tbank = 2;

		ptr = (__far char*)(program_categories[cat_tree[idx]].name + ((long)tbank << 16));

		// strcat(str, ptr) equivalent
		while (*ptr != 0)
		{
			str[str_idx] = *ptr;
			str_idx++;
			ptr++;
		}

		if (idx > 0)
		{
			str[str_idx] = '\x80'; str_idx++;
			str[str_idx] = ' '; str_idx++;
			str[str_idx] = '>'; str_idx++;
			str[str_idx] = '>'; str_idx++;
			str[str_idx] = ' '; str_idx++;
			str[str_idx] = '\x81'; str_idx++;
		}
	}
	str[str_idx] = '\0';
}

void program_drawcategoryheader()
{
	poke(&program_bounceselectionline, 1);

	fontsys_map();
	program_clearheader();
	program_settextbank(0);
	program_drawline((uint16_t)&headercategorytext, 0x00, 0, 0);
	program_settextbank(2);
	program_gencategory_tree();

	program_settextbank(0);
	program_drawline((uint16_t)str, 0x1f, 0, 2*21);
	fontsys_unmap();
}

void program_drawcategoryfooter()
{
	fontsys_map();
	program_clearfooters();
	program_settextbank(0);
	program_drawline((uint16_t)&footertext1, 0x00, 38, 11*2);
	program_drawline((uint16_t)&footertext2, 0x00, 40, 26*2);
	program_setcategorytextbank();
	fontsys_unmap();
}

void program_genfilename_and_author(void)
{
	uint8_t idx = 0;

	if (program_current_entry->full != 0)
		ptr = (__far char*)(program_current_entry->full + ((long)program_textbank << 16));
	else
		ptr = (__far char*)(program_current_entry->title + ((long)program_textbank << 16));

	// strcpy(str, ptr) equivalent
	while (*ptr != 0)
	{
		str[idx] = *ptr;
		idx++;
		ptr++;
	}

	if (program_current_entry->author == 0)
	{
		str[idx] = '\0';
		return;
	}

	str[idx] = '\x80'; idx++;
	str[idx] = ' '; idx++;
	str[idx] = '-'; idx++;
	str[idx] = ' '; idx++;
	str[idx] = '\x81'; idx++;

	ptr = (__far char*)(program_current_entry->author + ((long)program_textbank << 16));

	// strcat(str, ptr); equivalent
	while (*ptr != 0)
	{
		str[idx] = *ptr;
		idx++;
		ptr++;
	}
	str[idx] = '\0';
}


void program_drawentryheader()
{
	poke(&program_bounceselectionline, 0);

	fontsys_map();
	program_clearheader();
	program_settextbank(0);
	program_drawline((uint16_t)&headerentrytext, 0x00, 0, 0);
	program_setcategorytextbank();
	program_genfilename_and_author();
	program_settextbank(0);
	program_drawline((uint16_t)str, 0x2f, 0, 2*20);
	fontsys_unmap();
}

void program_drawentryfooter()
{
	fontsys_map();
	program_clearfooters();
	program_settextbank(0);
	program_drawline((uint16_t)&footertext1, 0x00, 38, 10*2);
	program_setcategorytextbank();

	ptr = (__far char*)(program_current_entry->title + ((long)program_textbank << 16));

	if(*ptr != 0)
	{
		program_settextbank(0);
		program_drawline((uint16_t)&footertext3, 0x00, 40, 22*2);
	}

	program_setcategorytextbank();
	fontsys_unmap();
}

void program_drawintroscreen()
{
	poke(&program_drawselectionline, 0);

	fontsys_map();

	// draw ID4 logo
	for(uint8_t y=0; y<ID4HEIGHT; y++)
	{
		for(uint8_t x=0; x<2*ID4WIDTH; x++)
		{
			poke(SCREEN+(y+15)*RRBSCREENWIDTH2+(x+44), peek(ID4SCREEN + y*2*ID4WIDTH + x));
			poke(0x8000+(y+15)*RRBSCREENWIDTH2+(x+44), peek(ID4ATTRIB + y*2*ID4WIDTH + x));
		}
	}

	program_settextbank(0); // set current text bank to 0

	program_drawintroheader();

	program_drawline((uint16_t)&introtext1, 0x00, 10, 26*2);
	program_drawline((uint16_t)&introtext2, 0x00, 22, 26*2);
	program_drawline((uint16_t)&introtext3, 0x00, 28, 9*2);

	program_drawintrofooter();

	fontsys_unmap();

	program_settextbank(2); // set current text bank to 2 to read categories correctly
}

void program_drawprogramentry(uint16_t row, uint8_t index)
{
	fnts_row = 2*row;
	fnts_column = 0;

	poke(&fnts_bottomlineadd1 + 1, 0x80);
	poke(&fnts_bottomlineadd2 + 1, 0);

	poke(&fnts_curpal + 1, 0x0f);

	poke(0x5c, peek(&fnts_lineptrlistlo + index));
	poke(0x5d, peek(&fnts_lineptrlisthi + index));

	fontsys_asm_setupscreenpos();
	fontsys_asm_render();
}

void program_drawcategoryentry(uint16_t row, uint8_t index)
{
	if(current_cat_idx == 0xff)
	{
		uint8_t color = 0x0f;

		// Highlight News and Credits in a different colour
		uint8_t cat = program_category_indices[index];
		if(cat >= program_numcategories-NUM_SPECIAL_CATS && cat < program_numcategories)
				color = 0x1f;	// blue

		// top level categories
		program_drawline(program_categories[program_category_indices[index]].name, color, 2 * row, 0 /* 40 */);
	}
	else
	{
		// below top level categories
		uint8_t color = 0x0f;
		if(program_entries[index].dir_flag != 0xff)
			color = 0x2f; // draw as yellow like original intro disk

		uint8_t curbank = program_textbank;

		if((current_cat_idx >= program_numcategories-NUM_SPECIAL_CATS && current_cat_idx < program_numcategories)
				|| current_cat_idx == DEMO_CAT_IDX) // force 'Demo' category items into 2nd bank too (for more space)
			program_settextbank(5); // set text bank to 5 for credits and news

		if(program_entries[index].dir_flag != 0xff)
			program_settextbank(2);	// force sub-directories text to bank 2

		ptr = (__far char *)(program_entries[index].title + ((long)program_textbank << 16));

		if (program_entries[index].title != 0 && ptr[0] == '-' && ptr[1] == '(')	// -(c)- items in red
			color = 0x1f;

		if(program_entries[index].full != 0)
			program_drawline(program_entries[index].full, color, 2 * row, 0 /* 40 */);
		else if(program_entries[index].title != 0)
			program_drawline(program_entries[index].title, color, 2 * row, 0 /* 40 */);

		program_settextbank(curbank);
	}
}

void program_drawtopline()
{
	fontsys_map();
	
	dma_runjob((__far char *)&dma_cleartoplinecolorram1);
	dma_runjob((__far char *)&dma_cleartoplinecolorram2);
	dma_runjob((__far char *)&dma_cleartoplinescreenram1);
	dma_runjob((__far char *)&dma_cleartoplinescreenram2);

	if(program_selectedrow - 8 >= 0)
	{
		uint8_t index = program_selectedrow - 8;
		uint8_t row = 1;
		if(current_ent_idx != 0xff)
			program_drawprogramentry(row, index);
		else
			program_drawcategoryentry(row, index);
	}

	fontsys_unmap();
}

void program_drawbottomline()
{
	fontsys_map();

	dma_runjob((__far char *)&dma_clearbottomlinecolorram1);
	dma_runjob((__far char *)&dma_clearbottomlinecolorram2);
	dma_runjob((__far char *)&dma_clearbottomlinescreenram1);
	dma_runjob((__far char *)&dma_clearbottomlinescreenram2);

	if(program_selectedrow + 9 < program_numtxtentries)
	{
		uint8_t index = program_selectedrow + 9;
		uint8_t row = 18;
		if(current_ent_idx != 0xff)
			program_drawprogramentry(row, index);
		else
			program_drawcategoryentry(row, index);
	}

	fontsys_unmap();
}

void program_movescreenup()
{
	fontsys_map();
	dma_runjob((__far char *)&dma_copycolorramup);
	dma_runjob((__far char *)&dma_copyscreenramup);
	fontsys_unmap();
}

void program_movescreendown()
{
	fontsys_map();
	dma_runjob((__far char *)&dma_copycolorramdown);
	dma_runjob((__far char *)&dma_copyscreenramdown);
	fontsys_unmap();
}

void program_loaddata()
{
	fl_init();
	fl_waiting();
	
	poke(&fl_mode, 0);												// set mode to IFFL loading
	floppy_iffl_fast_load_init("INTRODATA");
	floppy_iffl_fast_load(); 										// chars
	floppy_iffl_fast_load();										// palette
	floppy_iffl_fast_load();										// logo chars
	floppy_iffl_fast_load();										// logo screen
	floppy_iffl_fast_load();										// logo attrib
	floppy_iffl_fast_load();										// qranchor sprites 0
	floppy_iffl_fast_load();										// qranchor sprites 1
	floppy_iffl_fast_load();										// qranchor sprites 2
//	floppy_iffl_fast_load();										// unicorn sprites 0
//	floppy_iffl_fast_load();										// unicorn sprites 1
//	floppy_iffl_fast_load();										// unicorn sprites 2
//	floppy_iffl_fast_load();										// unicorn sprites 3
//	floppy_iffl_fast_load();										// unicorn sprites 4
//	floppy_iffl_fast_load();										// unicorn sprites 5
	floppy_iffl_fast_load();										// menudict.bin
	floppy_iffl_fast_load();										// menustrc.bin
	floppy_iffl_fast_load();										// menu.bin
	floppy_iffl_fast_load();										// menu2.bin
	floppy_iffl_fast_load();										// song.mod
	// poke(&fl_iffl_debug, 1);
	floppy_iffl_fast_load(); 										// QRspr         $e000
	floppy_iffl_fast_load();										// id4 chars     $5e000 - $1200
	floppy_iffl_fast_load();										// id4 screen    $0090
	floppy_iffl_fast_load();										// id4 attrib    $0090

	fl_exit();
}

void program_drawlogo()
{
	for(uint8_t i=0; i<64; i++)
		poke(0x8000+i, 0);

	for(uint8_t j=0; j<10; j++)
	{
		for(uint8_t i=0; i<RRBSCREENWIDTH; i++)
		{
			poke(LOGOFINALSCREEN+j*RRBSCREENWIDTH2+2*i+0, 0);		// clear with empty char ($0200*64=$8000). TODO - check if this is always safe
			poke(LOGOFINALSCREEN+j*RRBSCREENWIDTH2+2*i+1, 2);
			lpoke(LOGO_COLOR_RAM+j*RRBSCREENWIDTH2+2*i+0, 0);
			lpoke(LOGO_COLOR_RAM+j*RRBSCREENWIDTH2+2*i+1, 0);
		}
	}

	for(uint8_t i = 0; i < 80; i++)
	{
		poke(LOGOFINALSCREEN+0*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+0*80+i));
		poke(LOGOFINALSCREEN+1*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+1*80+i));
		poke(LOGOFINALSCREEN+2*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+2*80+i));
		poke(LOGOFINALSCREEN+3*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+3*80+i));
		poke(LOGOFINALSCREEN+4*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+4*80+i));
		poke(LOGOFINALSCREEN+5*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+5*80+i));
		poke(LOGOFINALSCREEN+6*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+6*80+i));
		poke(LOGOFINALSCREEN+7*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+7*80+i));
		poke(LOGOFINALSCREEN+8*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+8*80+i));
		poke(LOGOFINALSCREEN+9*RRBSCREENWIDTH2+i, peek(LOGOSCREEN+9*80+i));

		lpoke(LOGO_COLOR_RAM+0*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+0*80+i));
		lpoke(LOGO_COLOR_RAM+1*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+1*80+i));
		lpoke(LOGO_COLOR_RAM+2*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+2*80+i));
		lpoke(LOGO_COLOR_RAM+3*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+3*80+i));
		lpoke(LOGO_COLOR_RAM+4*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+4*80+i));
		lpoke(LOGO_COLOR_RAM+5*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+5*80+i));
		lpoke(LOGO_COLOR_RAM+6*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+6*80+i));
		lpoke(LOGO_COLOR_RAM+7*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+7*80+i));
		lpoke(LOGO_COLOR_RAM+8*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+8*80+i));
		lpoke(LOGO_COLOR_RAM+9*RRBSCREENWIDTH2+i, peek(LOGOATTRIB+9*80+i));
	}
}

void program_init()
{
	program_menubin_struct = (__far uint8_t*)(menubinaddr);
	program_numcategories = lpeek(program_menubin_struct);
	program_categories = (__far category*)(program_menubin_struct+1);

	program_numbasecategories = 0;
	for(uint8_t index = 0; index < program_numcategories; index++)
	{
		if(program_categories[index].parent_cat_idx == 0xff)
		{
			program_category_indices[program_numbasecategories] = index;
			program_numbasecategories++;
		}
	}

	current_cat_idx = 0xff;
	current_ent_idx = 0xff;
	program_numtxtentries = program_numbasecategories;

	modplay_init();
	fontsys_init();

	dma_runjob((__far char *)&dma_clearentiresafecolorram);
	
	dma_runjob((__far char *)&dma_clearfullcolorram1);
	dma_runjob((__far char *)&dma_clearfullcolorram2);
	dma_runjob((__far char *)&dma_clearfullscreen1);
	dma_runjob((__far char *)&dma_clearfullscreen2);

	program_drawlogo();

	modplay_initmod(SONGADDRESS);

	modplay_enable();

	c_textypos = verticalcenter + c_textyposstart;

	VIC2.BORDERCOL = 0x0f;
	VIC2.SCREENCOL = 0x0f;

	// VIC4.PALEMU		= 1;			// $d054 - turn on PALEMU

	VIC2.SE				= 0b00000000;	// 0b00000011;	// $d015 - disable all sprites. will be turned on after intro sequence
	VIC4.SPRPTRADR		= SPRITEPTRS;	// $d06c - location of sprite pointers
	VIC4.SPRPTR16		= 1;			// $d06e - 16 bit sprite pointers
	VIC2.BSP			= 0;			// $d01b - sprite background priority
	VIC4.SPRX64EN		= 0b11011111;	// $d057 - 64 pixel wide sprites
	VIC4.SPR16EN		= 0b11000111;	// $d06b - Full Colour Mode
	VIC4.SPRHGTEN		= 0b11011111;	// $d055 - enable setting of sprite height
	VIC4.SPRH640		= 0;			// $d054 - disable SPR640 for all sprites
	VIC4.SPRHGHT		= sprheight;	// $d056 - set sprite height to 64 pixels for sprites that have SPRHGTEN enabled
	VIC2.SEXX			= 0b00011000;	// $d01d - enable x stretch
	VIC2.SEXY			= 0b00011000;	// $d017 - enable y stretch

	VIC2.S0X			= 0;			// $d000 - anchor sprite positions
	VIC2.S0Y			= 180 + 4;
	VIC2.S1X			= 0;
	VIC2.S1Y			= 180 + 4;
	VIC2.S2X			= 0;
	VIC2.S2Y			= 180 + 4;

	VIC2.S3X			= 0;			// $d000 - anchor sprite positions
	VIC2.S3Y			= 180;			// $d001 - sprite 0 y position QR
	VIC2.S4X			= 0;			// $d000 - sprite 1 x position QR background
	VIC2.S4Y			= 180;			// $d001 - sprite 1 y position QR background

	VIC2.SPR0COL		= 0;			// $d027 - anchor sprite colours
	VIC2.SPR1COL		= 0;			//
	VIC2.SPR2COL		= 0;			//
	VIC2.SPR3COL		= 0x0f;			// $d02a - QR sprite colours
	VIC2.SPR4COL		= 0x06;			// 
	VIC2.SPR6COL		= 0;			// $d02d - unicorn sprite colours
	VIC2.SPR7COL		= 0;			//
	VIC4.SPRTILENLSB	= 0;			// disable sprite tiling
	VIC4.SPRTILENMSB	= 0;			// 
	VIC4.SPRXSMSBS		= 0b00000000;	// Sprite H640 X Super-MSBs

	VIC4.SPRBPMENLSB	= 0b0111;		// sprite bitplane mode on, so sprites 0,1,2 use palettes 8,9,10

	VIC2.SXMSB			= 0b00011111;	// $d010 - set x MSB bits

	VIC2.SPRMC0			= 0;
	VIC2.SPRMC1			= 0;

	VIC2.S6X			= 0;			// unicorn sprite 1 xpos
	VIC2.S6Y			= 250;			// unicorn sprite 1 ypos
	VIC2.S7X			= 15;			// unicorn sprite 2 xpos
	VIC2.S7Y			= 250;			// unicorn sprite 2 ypos

	//uint8_t spritenum = 0;
	//poke(SPRITEPTRS+12,  ((UNISPRITEDATA + spritenum*0x0400 + 0x0000) / 64) & 0xff);		// unicorn sprite pointers
	//poke(SPRITEPTRS+13, (((UNISPRITEDATA + spritenum*0x0400 + 0x0000) / 64) >> 8) & 0xff);
	//poke(SPRITEPTRS+14,  ((UNISPRITEDATA + spritenum*0x0400 + 0x0200) / 64) & 0xff);		// unicorn sprite pointers
	//poke(SPRITEPTRS+15, (((UNISPRITEDATA + spritenum*0x0400 + 0x0200) / 64) >> 8) & 0xff);

	poke(SPRITEPTRS+0,  ((QRANCHORSPRITEDATA + 0x0000) / 64)       & 0xff);		// qranchor sprite pointers
	poke(SPRITEPTRS+1, (((QRANCHORSPRITEDATA + 0x0000) / 64) >> 8) & 0xff);
	poke(SPRITEPTRS+2,  ((QRANCHORSPRITEDATA + 0x0200) / 64)       & 0xff);
	poke(SPRITEPTRS+3, (((QRANCHORSPRITEDATA + 0x0200) / 64) >> 8) & 0xff);
	poke(SPRITEPTRS+4,  ((QRANCHORSPRITEDATA + 0x0400) / 64)       & 0xff);
	poke(SPRITEPTRS+5, (((QRANCHORSPRITEDATA + 0x0400) / 64) >> 8) & 0xff);
	poke(SPRITEPTRS+6, ( (QRSPRITEDATA+(sprwidth/8)*sprheight)/64)       & 0xff);		// qr main sprite pointers
	poke(SPRITEPTRS+7, (((QRSPRITEDATA+(sprwidth/8)*sprheight)/64) >> 8) & 0xff);
	poke(SPRITEPTRS+8, ( (QRSPRITEDATA+0x000)/64) & 0xff);
	poke(SPRITEPTRS+9, (((QRSPRITEDATA+0x000)/64) >> 8) & 0xff);

	for(uint16_t i=0; i<0x1200-0x0400; i++)				// clear QR sprites
		poke(QRSPRITEDATA+i, 0);

	for(uint16_t i=0; i<sprheight*(sprwidth/8); i++)	// fill QR background sprite
		poke(QRSPRITEDATA+i, 255);

	poke(&textypos, c_textypos);

	program_drawintroscreen();

	poke(&mainlogoxposlo,   0x50);
	poke(&maintextxposlo,   0x50);
	poke(&headertextxposlo, 0x50);
	poke(&footertextxposlo, 0x50);
	poke(&mainlogoxposhi,   0);
	poke(&maintextxposhi,   0);
	poke(&headertextxposhi, 0);
	poke(&footertextxposhi, 0);

	fadepal_init(); // init fadepal to start increasing in irq_main
}

void program_build_linelist(uint16_t entry)
{
	poke(0x5c, (entry >> 0) & 0xff);
	poke(0x5d, (entry >> 8) & 0xff);

	poke(&program_mainloopstate, 1);	// set state machine to build line ptr list
}

void program_startdrawtextscreen()
{
	fontsys_map();

	fontsys_clearscreen();

	program_rowoffset = 0;
	
	int16_t startrow = 9 - program_selectedrow;
	if(startrow < 1)
	{
		program_rowoffset = -startrow+1;
		startrow = 1;
	}

	int16_t endrow = startrow + program_numtxtentries;

	if(program_numtxtentries - program_selectedrow < 13)
		endrow = 9 + (program_numtxtentries - program_selectedrow);

	if(endrow > 19)
		endrow = 19;

	program_deferredindex = program_rowoffset;
	program_deferredrow = startrow;
	program_deferredendrow = endrow;

	fontsys_unmap();
}

void program_updatetextsequence()
{
	program_maintextxpos = 800;
	program_startdrawtextscreen();
	poke(&program_mainloopstate, 3);
	poke(&program_nextmainloopstate, 30);
}

void program_afterintrosequence()
{
	fontsys_map();
	program_clearheader();
	program_clearfooters();
	fontsys_unmap();
	poke(&program_mainloopstate, 20);
	poke(&program_nextmainloopstate, 26);
}

void program_drawnexttextline()
{
	if(program_deferredrow < program_deferredendrow)
	{
		fontsys_map();

		if(current_ent_idx == 0xff)
		{
			program_drawcategoryentry(program_deferredrow, program_deferredindex);
			program_deferredindex++;
		}
		else
		{
			program_drawprogramentry(program_deferredrow, program_deferredindex);
			program_deferredindex++;
		}

		fontsys_unmap();
	}
	else
	{
		program_checkdrawQR();
		program_mainloopstate = program_nextmainloopstate;
		program_nextmainloopstate = 0;
	}

	program_deferredrow++;
}

void program_drawtextscreen()
{
	fontsys_map();

	fontsys_clearscreen();

	program_rowoffset = 0;
	
	int16_t startrow = 9 - program_selectedrow;
	if(startrow < 1)
	{
		program_rowoffset = -startrow+1;
		startrow = 1;
	}

	int16_t endrow = startrow + program_numtxtentries;

	if(program_numtxtentries - program_selectedrow < 13)
		endrow = 9 + (program_numtxtentries - program_selectedrow);

	if(endrow > 19)
		endrow = 19;

	uint8_t index = program_rowoffset;
	if(current_ent_idx == 0xff)
	{
		for(uint16_t row = startrow; row < endrow; row++)
		{
			program_drawcategoryentry(row, index);
			index++;
		}
	}
	else
	{
		for(uint16_t row = startrow; row < endrow; row++)
		{
			program_drawprogramentry(row, index);
			index++;
		}

		program_checkdrawQR();
	}

	fontsys_unmap();
}

void program_setcategory(uint8_t index)
{
	program_category_selectedrows[current_cat_idx+1] = program_selectedrow;

	program_selectedrow = program_category_selectedrows[index+1];

	current_cat_idx = index;
	uint16_t cat_entry_offset = program_categories[current_cat_idx].cat_entry_offset;
	program_numentries = lpeek(program_menubin_struct + cat_entry_offset + 0);
	program_entries = (__far catentry*)(menubinaddr + cat_entry_offset + 1);

	current_ent_idx = 0xff;

	if(current_cat_idx == 0xff)
		program_numtxtentries = program_numbasecategories;
	else
		program_numtxtentries = program_numentries;
}

int parse_custom_rom(uint32_t addr)
{
  uint8_t cnt = 0;
  char c;

  while ( (c = lpeek(addr)) != '-') // starts with '-'?
  {
    poke(&romfilename + cnt, c);
    addr++;
    cnt++;
  }
  poke(&romfilename + cnt, '\0');
  cnt++;

  return cnt;
}

void set_default_rom_filename()
{
	for(uint8_t i=0; i<11; i++)
		poke(&romfilename + i, defaultromstring[i]);
}

void set_autoboot_prg_filename()
{
	for(uint8_t i = 0; i<16; i++)
		poke(&prgfilename+i, autobootstring[i]);
}

void set_mega65_d81_filename()
{
	for(uint8_t i = 0; i<16; i++)
		poke(&mountname+i, mega65d81string[i]);
}

void set_intro4_d81_filename()
{
	for(uint8_t i = 0; i<16; i++)
		poke(&mountname+i, intro4d81string[i]);
}

void program_setintro4_names()
{
	set_default_rom_filename();
	set_intro4_d81_filename();
	set_autoboot_prg_filename();
}

void pressed_return_on_base_category(void)
{
  uint8_t bkp_ent_idx = current_ent_idx;

  program_setcategory(program_category_indices[program_selectedrow]);

  if (program_numtxtentries == 1)	// Only 1 subcategory, like 'Credits', so skip to entries straight away?
  {
    showing_credits = 1;
    current_ent_idx = 0;
    program_current_entry = &(program_entries[current_ent_idx]);
    program_selectedrow = 0;

    program_drawentryheader();
    program_drawentryfooter();

    if(program_current_entry->desc != 0)
      program_build_linelist(program_current_entry->desc);
  }
  else
  {
    // show sub-categories page
    program_drawcategoryheader();
    program_drawcategoryfooter();
    program_updatetextsequence();
  }
}

void program_main_processkeyboard()
{
	if(xemu_fudge > 0)
	{
		xemu_fudge--;
		return;
	}

	if(movedir != 0)
	{
		poke(&program_selectionframe, 0); // 60 is good for switching between menus, 0 for when scrolling

		if(movedir == 1) // moving down - text moves up
		{
			c_textyposoffset -= movespeed;
			c_textypos = verticalcenter + c_textyposstart + c_textyposoffset;
			if(c_textyposoffset <= 0)
			{
				c_textyposoffset = 0;
				c_textypos = (verticalcenter + c_textyposstart);
				movedir = 0;
			}
		}
		else if(movedir == -1) // moving up, text moves down
		{
			c_textyposoffset += movespeed;
			c_textypos = verticalcenter + c_textyposstart + c_textyposoffset;
			if(c_textyposoffset >= 16)
			{
				c_textyposoffset = 0;
				c_textypos = (verticalcenter + c_textyposstart);
				program_selectedrow--;
				program_movescreendown();
				program_checkdrawQR();
				movedir = 0;
			}
		}

		if(movedir != 0)
			return;
	}

	if(keyboard_modifierpressed(KEYBOARD_LEFTSHIFT) == 1 /* || keyboard_modifierpressed(KEYBOARD_RIGHTSHIFT) == 1 */)
	{
		movespeed = 8;
	}
	else
	{
		movespeed = 2;
	}

	if(keyboard_keypressed(KEYBOARD_CURSORDOWN) == 1)
	{
		if(program_state == 0)
			return;

		if(program_selectedrow == program_numtxtentries-1)
			return;

		c_textyposoffset = 16 - movespeed;
		c_textypos = verticalcenter + c_textyposstart + c_textyposoffset;
		program_selectedrow++;
		program_movescreenup();
		program_drawbottomline();
		program_checkdrawQR();
		movedir = 1;
		poke(&program_selectionframe, 0); // 60 is good for switching between menus, 0 for when scrolling
	}
	else if(keyboard_keypressed(KEYBOARD_CURSORUP) == 1)
	{
		if(program_state == 0)
			return;

		if(program_selectedrow == 0)
			return;

		program_drawtopline();
		movedir = -1;
		poke(&program_selectionframe, 0); // 60 is good for switching between menus, 0 for when scrolling
	}
	else if(keyboard_keyreleased(KEYBOARD_RETURN) || keyboard_keyreleased(KEYBOARD_CURSORRIGHT))
	{
		poke(&program_selectionframe, 60); // 60 is good for switching between menus, 0 for when scrolling

		if(program_state == 0)
		{
			program_state = 1;
			VIC2.SE	= 0b11000011; // enable the sprites
			//VIC2.SE	= 0b00000011; // enable the sprites
			program_afterintrosequence();
			return;
		}

		// pressed right arrow on a program/entry, just return
		if(keyboard_keyreleased(KEYBOARD_CURSORRIGHT) && current_ent_idx != 0xff)
			return;

		// pressed return on a program/entry? (get ready to mount/run it)
		if(keyboard_keyreleased(KEYBOARD_RETURN) && current_ent_idx != 0xff)
		{
			uint32_t titleaddr = (((uint32_t)program_textbank) << 16) + program_current_entry->title;

			// no mount or prg? must be credits or news or something, back out
			if(program_current_entry->mount == 0 && lpeek(titleaddr) == 0)
				return;

			// handle mounting/running of disk here

			uint8_t autoboot = 0;
			uint8_t go64flag = 0;
			uint8_t palflag = 0;
			uint8_t ntscflag = 0;

			if(program_current_entry->title != 0)
			{
				uint32_t addroffset = 0;

				set_default_rom_filename(); // start with default rom name

				// grrr, some weird calypsi bug I think, have to store this in another var

				// N.B. This simple check doesn't work any more for the silent enigma demo, because
				// it has this in the title: "-rom:999999.bin--boot-"
				while (lpeek(titleaddr + addroffset) == 0x2d) // starts with '-'?
				{
					uint32_t secondcharaddr = titleaddr + addroffset + 1;

					if(lpeek(secondcharaddr) == 'R') // -Rom:xxx-?
					{
						addroffset += parse_custom_rom(secondcharaddr + 4) + 4 + 1;
					}
					else if(lpeek(secondcharaddr) == '(') // -(c)-?
					{
						addroffset += 5;
					}
					else if (lpeek(secondcharaddr) == 'G') // -Go64-?
					{
						go64flag = 1;
						addroffset += 6;
					}
					else if(lpeek(secondcharaddr) == 'N') // -Ntsc-?
					{
						ntscflag = 1;
						addroffset += 6;
					}
					else if(lpeek(secondcharaddr) == 0x50) // -Pal-?
					{
						palflag = 1;
						addroffset += 5;
					}
					else if(lpeek(secondcharaddr) == 0x42) // -Boot-?
					{
						// assume no prg file name and just boot
						for(uint8_t i = 0; i<16; i++)
							poke(&prgfilename+i, 0);

						autoboot = 1;
						addroffset += 6;
					}
				}

				if(autoboot == 0)
				{
					// TODO - figure out why LPEEK is changing titleaddr
					titleaddr = (((uint32_t)program_textbank) << 16) + program_current_entry->title + addroffset;

					uint8_t i = 0;
					for(; i<16; i++)
					{
						uint8_t b = lpeek(titleaddr + i);
						poke(&prgfilename+i, b);
						if(b == 0)
							break;
					}
					for(; i<16; i++)
					{
						poke(&prgfilename+i, 0);
					}
				}
			}
			else
			{
				for(uint8_t i = 0; i<16; i++)
					poke(&prgfilename+i, 0);
			}

			if(program_current_entry->mount != 0)
			{
				uint32_t mountaddr = (((uint32_t)program_textbank) << 16) + program_current_entry->mount;

				uint8_t i = 0;
				for(; i<64; i++)
				{
					uint8_t b = lpeek(mountaddr + i);
					poke(&mountname+i, b);
					if(b == 0)
						break;
				}
				for(; i<64; i++)
				{
					poke(&mountname+i, 0);
				}
			}
			else
			{
				for(uint8_t i = 0; i<64; i++)
					poke(&mountname+i, 0);
			}

			if(program_current_entry->title != 0)
			{
				poke(&wasautoboot, autoboot);
				poke(&wasgo64flag, go64flag);
				poke(&wasntscflag, ntscflag);
				poke(&waspalflag, palflag);

				poke(&program_mainloopstate, 5);
				return;
			}
		}

		// did we press RETURN on a base category?
		if(current_cat_idx == 0xff)
		{
			pressed_return_on_base_category();
		}
		// did we press RETURN on a sub-category?
		else if (program_entries[program_selectedrow].dir_flag != 0xff)
		{
			program_setcategory(program_entries[program_selectedrow].dir_flag);
			program_drawcategoryheader();
			program_drawcategoryfooter();
			program_updatetextsequence();
		}
		// did we press RETURN on a menu item that should now show page details?
		else
		{
			// at sub-category level? then start to build linelist, but only if sub-category doesn't have even more sub-categories (check for 'dir' in gen.py for this)
			uint8_t dirflag = program_entries[program_selectedrow].dir_flag;
			if(dirflag == 0xff)
			{
				current_ent_idx = program_selectedrow;
				program_current_entry = &(program_entries[current_ent_idx]);
				program_selectedrow = 0;

				program_drawentryheader();
				program_drawentryfooter();

				if(program_current_entry->desc != 0)
					program_build_linelist(program_current_entry->desc);
			}
			else
			{
				// at main-category level. draw sub-category
				program_setcategory(dirflag);
				program_setcategorytextbank();
				program_updatetextsequence();
			}
		}
	}
	else if(keyboard_keyreleased(KEYBOARD_ESC) || keyboard_keyreleased(KEYBOARD_SLASH) || keyboard_keyreleased(KEYBOARD_CURSORLEFT))
	{
		if (showing_credits) {
			current_ent_idx = 0xff;
			showing_credits = 0;
			program_selectedrow = 0;
		}

		if(current_ent_idx != 0xff)
		{
			// we were looking at an entry, so move back to sub-categories
			// don't change current_cat_idx, because that isn't changing

			// leave selected row at the entry we were just at
			program_selectedrow = current_ent_idx;
			// flag that we're not looking at an entry any more
			current_ent_idx = 0xff;
			if(current_cat_idx == 0xff)
			{
				// if we're now at the top level, use basecategories
				program_drawmaincategoryheader();
				program_drawmaincategoryfooter();
				program_numtxtentries = program_numbasecategories;
			}
			else
			{
				program_drawcategoryheader();
				program_drawcategoryfooter();
				program_numtxtentries = program_numentries;
			}
		}
		else if(current_cat_idx != 0xff)
		{
			// we were not looking at an entry, so we must have been looking at sub-categories, so just move up to base categories.

			program_setcategory(program_categories[current_cat_idx].parent_cat_idx);

			if(current_cat_idx == 0xff) // are we back at the main categories?
			{
				program_drawmaincategoryheader();
				program_drawmaincategoryfooter();
			}
			else
			{
				program_drawcategoryheader();
				program_drawcategoryfooter();
			}

			program_setcategorytextbank();
		}
		else
		{
			// we're already at the main categories page, so don't allow people to go back
			return;
		}

		program_updatetextsequence();
		poke(&program_selectionframe, 60); // 60 is good for switching between menus, 0 for when scrolling
	}
	else if(keyboard_keyreleased(KEYBOARD_I))
	{
		// move to intro disk selection menu
		set_default_rom_filename();
		set_mega65_d81_filename();
		set_autoboot_prg_filename();

		poke(&wasautoboot, 1);

		dma_runjob((__far char *)&dma_clearfullcolorram1);
		dma_runjob((__far char *)&dma_clearfullcolorram2);
		dma_runjob((__far char *)&dma_clearfullscreen1);
		dma_runjob((__far char *)&dma_clearfullscreen2);

		poke(&program_mainloopstate, 10);
	}
	else if(keyboard_keyreleased(KEYBOARD_M))
	{
		modplay_toggleenable();
	}
	else
	{
		program_keydowndelay = 32;
		program_keydowncount = 0;
	}
}

void program_update()
{
	if(program_showingqrcode)
	{
		program_qrcodexpos -= 8;
		if(program_qrcodexpos < program_qrcodexposmin)
			program_qrcodexpos = program_qrcodexposmin;
	}
	else
	{
		program_qrcodexpos += 8;
		if(program_qrcodexpos > program_qrcodexposmax)
			program_qrcodexpos = program_qrcodexposmax;
	}

	VIC2.S0X = program_qrcodexpos + 4;									// top left anchor
	VIC2.S1X = program_qrcodexpos + 2*program_urlsprsize - 14 - 4;		// top right anchor
	VIC2.S2X = program_qrcodexpos + 4;									// bottom left anchor

	VIC2.S3X = program_qrcodexpos;
	VIC2.S4X = program_qrcodexpos;

	program_setmaintextxpos(program_maintextxpos);
	if(program_realhw)
		poke(&maintextxscale, program_maintextxscale);

	if(current_ent_idx != 0xff && program_realhw)
	{
		uint8_t squish = peek(&id4sine+((4*program_framelo) & 0xff));
		squish >>= 5;
		program_headertextxscale = 120 - squish;
		poke(&headertextxscale, program_headertextxscale);
		poke(&headertextxposlo, 88-squish);
	}
	else
	{
		poke(&headertextxposlo, 80);
		if(program_realhw)
			poke(&headertextxscale, 120);
	}

	if(program_mainloopstate == 0) // not waiting for anything, so do update
	{
		program_main_processkeyboard();
		poke(&textyposoffset, c_textyposoffset);
		poke(&textypos, c_textypos);
	}
	else if(program_mainloopstate == 2)
	{
		program_numtxtentries = fnts_numlineptrs;
		program_updatetextsequence();
	}
	else if(program_mainloopstate == 20) // bouncing out of intro screen
	{
		uint8_t bouncelo = peek(&id4bouncelo+program_bounceframe);
		uint8_t bouncehi = peek(&id4bouncehi+program_bounceframe);
		uint16_t bounce = 80 + (bouncehi << 8) + bouncelo;
		program_maintextxpos = bounce;
		uint8_t squish = peek(&id4squish+program_bounceframe);
		program_maintextxscale = squish;

		if(program_bounceframe == MAX_BOUNCE_FRAMES)
			program_mainloopstate = program_nextmainloopstate;
		else
			program_bounceframe++;
	}
	else if(program_mainloopstate == 25)
	{
		program_startdrawtextscreen();
		poke(&program_mainloopstate, 3); // start drawing all lines deferred
		poke(&program_nextmainloopstate, 30); // set next state to show text immediately
	}
	else if(program_mainloopstate == 26)
	{
		program_startdrawtextscreen();
		poke(&program_mainloopstate, 3); // start drawing all lines deferred
		poke(&program_nextmainloopstate, 31); // set next state to scroll text screen in
	}
	else if(program_mainloopstate == 3)
	{
		program_drawnexttextline();
	}	
	else if(program_mainloopstate == 5)
	{
		fadepal_init();						// start fading out
		poke(&fadepal_direction, 1);
		poke(&program_mainloopstate, 6); // we're fading out now. set next state to 6 which waits for the fadeout to complete
	}	
	else if(program_mainloopstate == 6)
	{
		if(!fadepal_complete)
		{
			if(fadepal_value < 0x80)
				poke(&audio_volume, fadepal_value);
			audio_applyvolume();

			return;
		}

		fontsys_map();

		dma_runjob((__far char *)&dma_clearfullcolorram1);
		dma_runjob((__far char *)&dma_clearfullcolorram2);
		dma_runjob((__far char *)&dma_clearfullscreen1);
		dma_runjob((__far char *)&dma_clearfullscreen2);

		program_settextbank(0); // set current text bank to 0

		// draw loading text
		program_drawline((uint16_t)&loadingtext1, 0x00, 0, 2*0);
		program_drawline((uint16_t)&mountname,    0x00, 0, 2*10);
		
		program_drawline((uint16_t)&loadingtext2, 0x00, 3, 2*0);
		program_drawline((uint16_t)&prgfilename,  0x00, 3, 2*10);
		
		program_drawline((uint16_t)&loadingtext3, 0x00, 25, 2*34);

		if (wasntscflag)
			program_drawline((uint16_t)&loadingntsc, 0x00, 6, 2*0);

		if (waspalflag)
			program_drawline((uint16_t)&loadingpal, 0x00, 6, 2*0);

		fontsys_unmap();

		poke(&program_mainloopstate, 10);
	}
	else if(program_mainloopstate == 30)
	{
		program_maintextxpos = 80;
		program_mainloopstate = program_nextmainloopstate;
	}
	else if(program_mainloopstate == 31)
	{
		poke(&program_nextmainloopstate, 32); // set next state to settle down

		uint8_t bouncelo = peek(&id4bouncelo+program_bounceframe);
		uint8_t bouncehi = peek(&id4bouncehi+program_bounceframe);
		uint16_t bounce = 80 + (bouncehi << 8) + bouncelo;
		program_maintextxpos = bounce;
		uint8_t squish = peek(&id4squish+program_bounceframe);
		program_maintextxscale = squish;

		if(program_bounceframe == 0)
			program_mainloopstate = program_nextmainloopstate;
		else
			program_bounceframe--;
	}
	else if(program_mainloopstate == 32)
	{
		program_mainloopstate = 0;
		poke(&program_drawselectionline, 1);
		program_drawmaincategoryheader();
		program_drawmaincategoryfooter();
	}
}
