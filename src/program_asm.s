			.rtmodel cpu, "*"
			
			.extern fontsys_buildlineptrlist
			.extern fontsys_buildlineptrlist_end
			.extern fl_mode
			.extern fl_set_filename
			.extern fl_get_endofbasic
			.extern floppy_fast_load_init
			.extern floppy_fast_load
			.extern fadepal_increase_done
			.extern _Zp

			.extern audio_restore_master_volumes

			.extern verticalcenter

			.extern fontsys_asm_render
			.extern fontsys_asm_setupscreenpos
			.extern fontsys_asm_init
			.extern fnts_row
			.extern fnts_column

			.extern program_setintro4_names

; ------------------------------------------------------------------------------------

			.public program_realhw
program_realhw	.byte 0

; ------------------------------------------------------------------------------------

			.public program_mainloopstate
program_mainloopstate:
			.byte 0

			.public program_nextmainloopstate
program_nextmainloopstate:
			.byte 0

			;  0 = idle
			;  1 = build lineptrlist and QR code sprites
			;  2 = lineptrlist is done, but we're still waiting for the text to be rendered by the IRQ
			;  3 = render next line deferred
			;  5 = start fading out before we start a selected program
			; 10 = mount d81, load prg, patch vectors, reset, etc.
			; 20 = move text screen right
			; 25 = clear text screen and start deferred line rendering
			; 26 = start 'after intro' sequence.
			; 27 = start 'after program selection sequence'
			; 30 = move text screen left

; ------------------------------------------------------------------------------------

			; this is the loop that runs OUTSIDE the IRQs

			.public program_mainloop
program_mainloop:
			lda program_mainloopstate
			beq program_mainloop
			cmp #2							; 2 = lineptrlist is done, but we're still waiting for the text to be rendered, so continue loop
			beq program_mainloop
			cmp #1							; 1 = build lineptrlist
			bne pml2$
			jsr fontsys_buildlineptrlist
			jsr fontsys_buildlineptrlist_end
			lda #2							; 2 = lineptrlist is done, signal IRQ that it can start rendering disk
			sta program_mainloopstate
			jmp program_mainloop
pml2$:		cmp #10							; mount d81, load prg, patch vectors, reset, etc.
			bne program_mainloop
			jmp program_reset

; ------------------------------------------------------------------------------------

program_fakewait:

		ldx #0x80
pfw1$:	bit 0xd011
		bmi pfw1$
pfw2$:	bit 0xd011
		bpl pfw2$
		dex
		bne pfw1$		
		rts

; ------------------------------------------------------------------------------------

stop_audio:

		lda #0x00
		sta 0xd711							; disable audio DMA
		sta 0xd720							; stop audio playback
		sta 0xd730
		sta 0xd740
		sta 0xd750
		rts

; ------------------------------------------------------------------------------------

hyppo_error:

		pha									; store A,X for error border colour later.
		phx

		lda #0x38							; mount failed. call hyppo_geterrorcode and store A in $c000 for debugging. 
		sta 0xd640							; trying to mount midnightmega.d81 is returning error code $88 (file not found)
		clv

		plx
		pla

hyppo_error_loop:

		sta 0xd020
		stx 0xd020
		jmp hyppo_error_loop

; ------------------------------------------------------------------------------------

irq_load
		php
		pha
		phx
		phy
		phz

		;inc 0xd020
		asl 0xd019							; acknowledege (raster) IRQ

		plz
		ply
		plx
		pla
		plp
		rti

; ------------------------------------------------------------------------------------

; logic copied from nobato's easterbunnydemo2:
; https://discord.com/channels/719326990221574164/1195701571955216434/1198550236939960340

switch_ntsc:
 ; 1020 poke $ffd306f,$87
		lda #0x87
		sta 0xd06f

 ; 1030 poke $ffd3072,$18
 		lda #0x18
		sta 0xd072

 ; 1040 poke $ffd3048,$2a
 		lda #0x2a
		sta 0xd048

 ; 1050 poke $ffd3049,$00 or (peek($ffd3049) and $f0)
		lda 0xd049
		and #0xf0
		ora #0x00
		sta 0xd049

 ; 1060 poke $ffd304a,$b9
 		lda #0xb9
		sta 0xd04a

; 1070 poke $ffd304b,$01 or (peek($ffd304b) and $f0)
		lda 0xd04b
		and #0xf0
		ora #0x01
		sta 0xd04b

; 1080 poke $ffd304e,$2a
		lda #0x2a
		sta 0xd04e

; 1090 poke $ffd304f,$00 or (peek($ffd304f) and $f0)
		lda 0xd04f
		and #0xf0
		ora #0x00
		sta 0xd04f

; 1100 poke $ffd3072,$1b :rem 24=$1b or $24
		lda #0x1b
		sta 0xd072

; 1110 poke $ffd3c0e,peek($ffd3c0e) or $7f
		lda 0xdc0e
		ora #0x7f
		sta 0xdc0e

; 1120 poke $ffd3d0e,peek($ffd3d0e) or $7f
		lda 0xdd0e
		ora #0x7f
		sta 0xdd0e

		rts

; ------------------------------------------------------------------------------------

switch_pal:
; 2040 poke $ffd306f,$00
		lda #0x00
		sta 0xd06f

; 2050 poke $ffd3072,$00
		lda #0x00
		sta 0xd072

; 2060 poke $ffd3048,$68
		lda #0x68
		sta 0xd048

; 2070 poke $ffd3049,$00 or (peek($ffd3049) and $f0)
		lda 0xd049
		and #0xf0
		sta 0xd049

; 2080 poke $ffd304a,$f8
		lda #0xf8
		sta 0xd04a

; 2090 poke $ffd304b,$01 or (peek($ffd304b) and $f0)
		lda 0xd04b
		and #0xf0
		ora #0x01
		sta 0xd04b

; 2100 poke $ffd304e,$68
		lda #0x68
		sta 0xd04e

; 2110 poke $ffd304f,$00 or (peek($ffd304f) and $f0)
		lda 0xd04f
		and #0xf0
		ora #0x00
		sta 0xd04f

; 2120 poke $ffd3072,$00
		lda #0x00
		sta 0xd072

; 2130 poke $ffd3c0e,peek($ffd3c0e) or $80
		lda 0xdc0e
		ora #0x80
		sta 0xdc0e

; 2140 poke $ffd3d0e,peek($ffd3d0e) or $80
		lda 0xdd0e
		ora #0x80
		sta 0xdd0e

		rts

; ------------------------------------------------------------------------------------

check_for_ntsc_pal_switching:
		; check if we need to switch from pal-to-ntsc
check_pal_to_ntsc:
		lda wasntscflag
		beq check_ntsc_to_pal
		bit 0xd06f
		bmi check_ntsc_to_pal
		jsr switch_ntsc
		rts

check_ntsc_to_pal:
		lda waspalflag
		beq bail_out
		bit 0xd06f
		bpl bail_out
		jsr switch_pal

bail_out:
	rts

romtext:
		.asciz "\x84\xa1rom NOT FOUND\xa2: "
mounttext:
		.asciz "\x84\x1mount NOT FOUND\xa2!"
prgtext:
		.asciz "\x84\x1prg NOT FOUND\xa2!"

mapin_stuff:
		; map in stuff
		lda #0xff
		ldx #0b00000000
		ldy #0xff
		ldz #0b00001111
		map
		eom

		lda #0x00
		ldx #0b00000000
		ldy #0x88
		ldz #0x17
		map
		eom
		rts


		.public prg_failed
prg_failed:
		sei
		lda #0x35
		sta 0x01

		lda #0x00
		sta program_mainloopstate

		jsr fontsys_asm_init

		jsr mapin_stuff

		lda #.byte0 prgtext
		sta 0x5c
		lda #.byte1 prgtext
		sta 0x5d
		lda #0x00
		sta 0x5e

		lda #0x06
		sta fnts_row
		lda #0x00
		sta fnts_column
		jsr fontsys_asm_setupscreenpos
		jsr fontsys_asm_render

		jmp reload_intro4

mount_failed:
		lda #0x35
		sta 0x01

		lda #0x00
		sta program_mainloopstate

		jsr fontsys_asm_init

		jsr mapin_stuff

		lda #.byte0 mounttext
		sta 0x5c
		lda #.byte1 mounttext
		sta 0x5d
		lda #0x00
		sta 0x5e

		lda #0x06
		sta fnts_row
		lda #0x00
		sta fnts_column
		jsr fontsys_asm_setupscreenpos
		jsr fontsys_asm_render

		jmp reload_intro4

romload_failed:
		lda #0x35
		sta 0x01

		lda #0x00
		sta program_mainloopstate

		jsr fontsys_asm_init

		jsr mapin_stuff

		lda #.byte0 romtext
		sta 0x5c
		lda #.byte1 romtext
		sta 0x5d
		lda #0x00
		sta 0x5e

		lda #0x06
		sta fnts_row
		lda #0x00
		sta fnts_column
		jsr fontsys_asm_setupscreenpos
		jsr fontsys_asm_render

		lda #.byte0 romfilename
		sta 0x5c
		lda #.byte1 romfilename
		sta 0x5d

		lda #0x20
		sta fnts_column
		jsr fontsys_asm_render
		; lda #0x00
		; tax
		; tay
		; taz
		; map
		; eom

		jmp reload_intro4

clear_bank0:
		; load up c64run at $4,2000
		sta 0xd707							; inline DMA copy
		.byte 0x80, 0 ; sourcemb
		.byte 0x81, 0	; destmb
		.byte 0x00							; end of job options
		.byte 0x03							; fill
		.word 0x2000						; count
		.word 0x0000						; src (fill value)
		.byte 0x00							; src bank (ignored)
		.word 0xe000						; dst
		.byte 0x00							; dst bank
		.byte 0x00							; cmd hi
		.word 0x0000						; modulo, ignored
		rts

reload_intro4
		jsr program_fakewait
		jsr program_setintro4_names
		lda #0x01
		sta wasautoboot
		lda #0x00
		sta wasgo64flag
		sta wasntscflag
		sta waspalflag
		jmp program_reset

; ------------------------------------------------------------------------------------

		.public romfilename
romfilename:
		.asciz "FOO.ROM" ; "MEGA65.ROM"
		.space 48, 0x00	; add more length, in-case overriden with longer rom filename

		.public prgfilename
prgfilename:
		.space 17

		.public mountname
mountname:		
		.space 65

		.public program_reset
program_reset:

		sei

		lda #0x35
		sta 0x01

		lda #0x00							; turn off (QR) sprites
		sta 0xd015

		jsr stop_audio

		lda #0x0f							; black borders
		sta 0xd020
		sta 0xd021

		lda #0b00010000						; enable screen
		tsb 0xd011

		lda verticalcenter+0
		sta 0xd04e							; VIC4.TEXTYPOSLSB
		lda #0x00
		sta 0xd04f							; VIC4.TEXTYPOSMSB

		lda #.byte0 0x0800					; COLOR_RAM_OFFSET
		sta 0xd064
		lda #.byte1 0x0800
		sta 0xd065

		lda #48								; DISPROWS
		sta 0xd07b

		lda #.byte0 0xa000					; SCREEN RRBSCREENWIDTH2
		sta 0xd060
		lda #.byte1 0xa000
		sta 0xd061

		lda #80								; set TEXTXPOS to default value and same as SDBDRWDLSB
		sta 0xd04c
		sta 0xd05c

		jsr fadepal_increase_done			; set palette to full

		lda #0x7f							; disable CIA interrupts (mainly to stop audio IRQs from firing)
		sta 0xdc0d
		sta 0xdd0d
		lda 0xdc0d							; and acknowledge any pending ones
		lda 0xdd0d

		lda #0x00							; disable IRQ raster interrupts
		sta 0xd01a

		lda #0xf8
		sta 0xd012
		lda #.byte0 irq_load
		sta 0xfffe
		lda #.byte1 irq_load
		sta 0xffff

		lda #0x01							; reenable IRQ raster interrupts
		sta 0xd01a

		cli

		lda program_realhw
		bne skip_xemuwait
		jsr program_fakewait

skip_xemuwait:
		lda mountname						; set d81 mount name if there is one
		beq try_prg_load

		lda #0x42							; unmount current images
		sta 0xd640
		clv

		ldx #0x3f
mntlp:	lda mountname,x
		sta 0x0200,x
		dex
		bpl mntlp
		
		ldy #0x02							; set d81 filename from 0x0200
		ldx #0x00							; X=0 -> xemu hdos fudge
		lda #0x2e							; hyppo_setname
		sta 0xd640
		clv
		
		lda #0x40							; hyppo_d81attach0 - attach d81 image
		sta 0xd640
		clv

		bcs try_prg_load
		jmp mount_failed

try_prg_load:

		lda prgfilename
		bne continueprgload
		jmp ready_reset

continueprgload
		lda #0x01							; Set fileload mode to non-IFFL
		sta fl_mode

		lda #.byte0 prgfilename
		sta _Zp+0
		lda #.byte1 prgfilename
		sta _Zp+1

		jsr fl_set_filename
		jsr floppy_fast_load_init			; set load address to $50000 and set fastload_request to 1
		jsr floppy_fast_load

		jsr fl_get_endofbasic				; get end-of-basic in X (lo) and Y (hi) ; top-of-basic for hangthedj = $457c
		stx endofbasic_backup+0
		sty endofbasic_backup+1

ready_reset:

		sei

		lda #0x37
		sta 0x01

		lda #0x00							; unmap upper 8 bits
		ldx #0x0f
		ldy #0x00
		ldz #0x0f
		map

		lda #0x00							; unmap lower 20 bits
		ldx #0x00
		ldy #0x00
		ldz #0x00
		map
		eom

		jsr audio_restore_master_volumes

		lda #0x00							; clear INTRO4.D81 string
		sta 0x11b2

		lda #0x02							; Disable C65 ROM write protection via Hypervisor trap
		sta 0xd641
		clv
		
		ldx #0xff							; copy rom filename to bank 0
prsfn$:	inx
		lda romfilename,x
		sta 0x0200,x
		bne prsfn$
		
		ldy #0x02							; set rom filename
		ldx #0x00							; X=0 -> xemu hdos fudge
		lda #0x2e
		sta 0xd640
		clv
		
		lda #0x00							; load rom file to $20000
		tax
		tay
	    ldz #0x02
	    lda #0x36
	    sta 0xd640
	    clv

		bcs romloaded

romnotloaded:
		jmp romload_failed

romloaded:

check_go64flag:
		lda wasgo64flag
		beq skip_c64run

		; load up c64run at $4,2000
		sta 0xd707							; inline DMA copy
		.byte 0x80, 0 ; sourcemb
		.byte 0x81, 0	; destmb
		.byte 0x00							; end of job options
		.byte 0x00							; copy
		.word (c64runend - c64run) + 1 	; count
		.word c64run						; src
		.byte 0x00							; src bank
		.word 0x2000						; dst
		.byte 0x04							; dst bank
		.byte 0x00							; cmd hi
		.word 0x0000						; modulo, ignored

skip_c64run:
		lda #0b00100000						; Disable $c000 mapping via $d030 as we want to write to interface rom.
		trb 0xd030							; Writing to rom is not possible via $d030. We'll use map for writing instead.

		lda #0xff							; unmap maphmb, while keeping maplmb
		ldx #0x0f
		ldy #0x00
		ldz #0x0f
		map
		eom

		lda #0x00							; mapping of interface rom $2c000 at $c000
		ldx #0x00							; (enables writing to rom, but also hides i/o for now as a side effect)
		ldy #0x00
		ldz #0x42
		map
		eom

		lda #0x07							; put basic IRQ vector (32007) in $80
		sta 0x80
		lda #0x20
		sta 0x81
		lda #0x03
		sta 0x82
		lda #0x00
		sta 0x83

		ldz #0x00							; store original basic IRQ vector (80 7b 4c 39)
		ldq [0x80],z
		stq basic_irq_backup

prepare_mega65_autostart_routine:
		ldx #0x00							; copy autostart routine to $c700. in xemu check: d 2c700
carc700:	lda runmeafterreset,x
		sta 0xc700,x
		inx
		bne carc700

patch_basic_irq_vector:
		ldz #0x00							; patch basic IRQ vector. in xemu : d 32007
		lda #.byte0 0xc700
		sta [0x80],z
		inz
		lda #.byte1 0xc700
		sta [0x80],z

		lda #0x00							; unmap $c000 rom to make I/O available again
		ldx #0x8d
		ldy #0x00
		ldz #0x00
		map
		eom

		lda #0b00100000						; activate $c000 rom read-only again via $d030
		tsb 0xd030
		
		lda #0x00							; restore rom write protection
		sta 0xd641
		clv

		lda #0x00							; set basepage to zeropage
		tab

		lda #0x82							; unmap SD sector buffer
		sta 0xd680

		lda #0b10000000						; Set bit 7 - HOTREG
		tsb 0xd05d

		ldx #0x00
d000fill:
		cpx #0x2f							; skip knock-register $d02f
		beq skipbadregs
		cpx #0x73							; skip RASTERHEIGHT/ALPHADELAY
		beq skipbadregs
		cpx #0x54							; skip $d054 for now and handle later to exclude PALEMU bit
		beq skipbadregs
		cpx #0x30							; skip rom states
		beq skipbadregs
		cpx #0x67							; skip (undocumented) SBPDEBUG
		beq skipbadregs
		lda d000table,x
		sta 0xd000,x
skipbadregs:
		inx
		cpx #0x7e							; should be enough to fill values up to $d07d?
		bne d000fill

		jsr check_for_ntsc_pal_switching

		lda #0b10011111						; turn off everything in $d054, except VFAST. Also don't touch PALEMU
		trb 0xd054
		lda #0b01000000						; turn VFAST on
		tsb 0xd054

		;lda #0x00							; THIS BREAKS YAMP65 IN A BAD WAY
		;sta 0xd073							; 0xd073 ALPHADELAY Alpha delay for compositor (1-16), RASTERHEIGHT (physical rasters per VIC-II raster (1 to 16))

		lda #0b10010111
		trb 0xd054							; disable Super-Extended Attribute Mode

		jsr clear_bank0
/*
		lda #0x00							; reset I/O to C64 mode
		sta 0xd02f

		lda #0x3f							; default C64 banking
		sta 0x00
		sta 0x01

		ldx #0xff							; default stack location
		ldy #0x01
		txs
		tys

		see									; only use 8-bit stack

		lda #0x40							; disable force_fast CPU mode
		sta 0x00
*/

		lda #0x00							; do final unmap and perform reset
		ldx #0x0f
		ldy #0x00
		ldz #0x0f
		map

		lda #0x00
		ldx #0x00
		ldy #0x00
		ldz #0x00
		map
		eom

		lda mountname						; if there was a mount name then don't unmount anything, because the prg on the d81 might need the disk
		bne final_reset

		lda #0x42							; unmount all images before reset
		sta 0xd640
		clv

final_reset:
		jmp (0xfffc)

runmeafterreset:

		sei

		lda 0xc700 + (wasautoboot-runmeafterreset)
		bne skipcopy						; don't copy memory if this was an autoboot

		lda 0xc700 + (wasgo64flag-runmeafterreset)
		bne c64run_afterreset

		sta 0xd707							; inline DMA copy
		.byte 0x80, (0x00050000 >> 20)		; sourcemb
		.byte 0x81, (0x00000000 >> 20)		; destmb
		.byte 0x00							; end of job options
		.byte 0x00							; copy
		.word 0xd6ff						; count												WAS A700 ($c700-$2000)!!!
		.word 0x0002						; src (skip load address)
		.byte (0x00050000 >> 16)			; src bank
		.word 0x2001						; dst
		.byte (0x00000000 >> 16)			; dst bank
		.byte 0x00							; cmd hi
		.word 0x0000						; modulo, ignored

		; clear $50000-$5ffff memory if there was graphics at $57d00 corrupting the hyppo mega65 logo
		;sta 0xd707							; inline DMA copy
		;.byte 0x80, (0x00050000 >> 20)		; sourcemb
		;.byte 0x81, (0x00000000 >> 20)		; destmb
		;.byte 0x00							; end of job options
		;.byte 0x01							; fill
		;.word 0xffff						; count												WAS A700 ($c700-$2000)!!!
		;.word 0x0000						; fill value
		;.byte 0							; fill value
		;.word 0x0000						; dst
		;.byte (0x00050000 >> 16)			; dst bank
		;.byte 0x00							; cmd hi
		;.word 0x0000						; modulo, ignored

		bra skipcopy


c64run_afterreset:
		; write basic stub into memory to call C64RUN

		sta 0xd707							; inline DMA copy
		.byte 0x80, 0 ; sourcemb
		.byte 0x81, 0	; destmb
		.byte 0x00							; end of job options
		.byte 0x00							; copy
		.word (endofbasic_backup - c64run_basic_stub) + 1 	; count
		.word 0xc700 + (c64run_basic_stub-runmeafterreset)	; src
		.byte 0x02							; src bank
		.word 0x2001						; dst
		.byte 0x00							; dst bank
		.byte 0x00							; cmd hi
		.word 0x0000						; modulo, ignored

skipcopy:

		lda #0x07							; restore basic IRQ vector.
		sta 0xf8
		lda #0x20
		sta 0xf9
		lda #0x03
		sta 0xfa
		lda #0x00
		sta 0xfb

		lda #0x02							; Disable C65 ROM write protection to restore basic IRQ vector
		sta 0xd641
		clv

		ldz #0x00							; Restore basic IRQ vector test in xemu: d 32007
		lda 0xc700 + (basic_irq_backup-runmeafterreset) + 0
		sta [0xf8],z
		inz
		lda 0xc700 + (basic_irq_backup-runmeafterreset) + 1
		sta [0xf8],z

		lda #0x00							; basic IRQ vector has been restored - restore rom write protection
		sta 0xd641
		clv

		lda 0xc700 + (wasautoboot-runmeafterreset)
		bne skipsetendofbasic				; don't set end of basic if this was an autoboot

		lda 0xc700 + (endofbasic_backup-runmeafterreset) + 0	; set end of basic
		sta 0x82
		lda 0xc700 + (endofbasic_backup-runmeafterreset) + 1	; set end of basic
		sta 0x83

skipsetendofbasic:

		ldx #0x0a
		stx 0x11b1
samis	lda 0xc700 + (intro5d81-runmeafterreset),x				; set automount INTRO5.D81 string for basic to process when reset is hit
		sta 0x11b2,x
		dex
		bpl samis

		lda 0xd60f												; add fake wait when running in xemu
		lsr a
		lsr a
		lsr a
		lsr a
		lsr a
		and #0x01
		bne skipexuwait2
		ldx #0x40
pfw1$:	bit 0xd011
		bmi pfw1$
pfw2$:	bit 0xd011
		bpl pfw2$
		dex
		bne pfw1$		
		
skipexuwait2:

		lda 0xc700 + (wasautoboot-runmeafterreset)
		bne skiprun												; don't issue run command if this was an autoboot disk

		lda #0x52	; R
		sta 0x02b0
		lda #0x55	; U
		sta 0x02b1
		lda #0x4e	; N
		sta 0x02b2
		lda #0x0d	; <cr>
		sta 0x02b3
		lda #0x04	; ndx - index to keyboard queue
		sta 0xd0

skiprun:

		cli
		rts
		;jmp 0x2006

basic_irq_backup:
		.long 0xbeefbeef

c64run_basic_stub:
		.word 0x200d	; pointer to next basic line
		.word 0x0001  ; line number = 1
		.byte 0x9e		; SYS token
		.ascii "$42000"	; location of kibo's C64RUN utility
		.byte 0x00		; end of basic line marker
		.word 0x0000	; end of program token?

endofbasic_backup:
		.word 0

		.public wasautoboot
wasautoboot:
		.byte 0

		.public wasgo64flag
wasgo64flag:
		.byte 0
		
		.public wasntscflag
wasntscflag:
		.byte 0

		.public waspalflag
waspalflag:
		.byte 0

intro5d81
		.byte 0x49, 0x4e, 0x54, 0x52, 0x4f, 0x35, 0x2e, 0x44, 0x38, 0x31, 0x00
		.space 29

; ------------------------------------------------------------------------------------

; Adding kibo's 'c64run.prg' file here as-is
; (as I didn't want to spend time porting from KickAss to Calyspi :D)
; - I've also put a copy of his KickAss source for this into "src/c64run.asm"
c64run:
		.incbin "exe/c64run.prg"
c64runend:

; ------------------------------------------------------------------------------------

d000table	; <dbg>M 777d000
		.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		.byte 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0xC9, 0x00, 0x24, 0x70, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00
		.byte 0x06, 0x06, 0x01, 0x02, 0x03, 0x01, 0x02, 0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x53
		.byte 0x64, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		.byte 0x01, 0x78, 0x01, 0x78, 0x01, 0x78, 0x01, 0x78, 0x68, 0x00, 0xF8, 0x01, 0x4F, 0x00, 0x68, 0x00
		.byte 0x00, 0x81, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x50, 0x00, 0x78, 0x01, 0x50, 0xC0, 0x50, 0x00
		.byte 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0xF8, 0x0F, 0x00, 0x00
		.byte 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xd2, 0xFF, 0xFF, 0x7F
		.byte 0x08, 0x00, 0x00, 0x2A, 0x27, 0x02, 0x00, 0x4C, 0xFF, 0x60, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
