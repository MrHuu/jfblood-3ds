/*
 Copyright (C) 2021 Szilard Biro
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */
 
/**
 * Paula output driver for MultiVoc
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <devices/audio.h>
#include <hardware/custom.h>
#include <hardware/intbits.h>
#include <hardware/dmabits.h>
#include <hardware/cia.h>
#include <graphics/gfxbase.h>
#include <proto/graphics.h>

#include <SDI_interrupt.h>
#include <SDI_compiler.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver_paula.h"

enum {
	PaulaErr_Warning = -2,
	PaulaErr_Error   = -1,
	PaulaErr_Ok      = 0,
	PaulaErr_Uninitialised,
	PaulaErr_OpenDevice,
	PaulaErr_IORequest,
	PaulaErr_MsgPort,
	PaulaErr_ChipMem,
	PaulaErr_NewGen,
};

static int ErrorCode = PaulaErr_Ok;
static int Initialised = 0;
static int Playing = 0;
static int actsound = 0;
static struct Interrupt *oldInt = NULL;
static struct MsgPort *audioMP = NULL;
static struct IOAudio *audioIO = NULL;
static UWORD period = 0;
static int channels = 0;
static char *chipBuffer;
static int channelSize;
static UBYTE oldFilter;

static struct CIA *ciaa = (struct CIA *)0xbfe001;
extern struct Custom custom;

static char *MixBuffer = 0;
static int MixBufferSize = 0;
static int MixBufferCount = 0;
static int MixBufferCurrent = 0;
static int MixBufferUsed = 0;
static void ( *MixCallBack )( void ) = 0;

INTERRUPTPROTO(AudioFunc, ULONG, struct Custom *c, APTR data)
{
	char *bufferLeft, *bufferRight;
	int remaining;
	int len;
	char *ptr, *sptr;

	actsound ^= 1;
	bufferLeft = chipBuffer + actsound * channelSize * 2;
	bufferRight = bufferLeft + ((channels > 1) ? channelSize : 0);
	remaining = MixBufferSize;
	ptr = bufferLeft;

	while (remaining > 0) {
		if (MixBufferUsed == MixBufferSize) {
			MixCallBack();
			
			MixBufferUsed = 0;
			MixBufferCurrent++;
			if (MixBufferCurrent >= MixBufferCount) {
				MixBufferCurrent -= MixBufferCount;
			}
		}
		
		while (remaining > 0 && MixBufferUsed < MixBufferSize) {
			sptr = MixBuffer + (MixBufferCurrent * MixBufferSize) + MixBufferUsed;
			
			len = MixBufferSize - MixBufferUsed;
			if (remaining < len) {
				len = remaining;
			}

			CopyMem(sptr, ptr, len);

			ptr += len;
			MixBufferUsed += len;
			remaining -= len;
		}
	}

	// switch to the new buffers
	custom.aud[0].ac_ptr = (UWORD *)bufferLeft;
	custom.aud[1].ac_ptr = (UWORD *)bufferRight;
	// clear the interrupt
	custom.intreq = INTF_AUD0;

	return 0;
}
MakeInterruptPri(AudioInt, AudioFunc, "MultiVoc audio interrupt", NULL, 100);

int PaulaDrv_GetError(void)
{
    return ErrorCode;
}

const char *PaulaDrv_ErrorString( int ErrorNumber )
{
    const char *ErrorString;

    switch( ErrorNumber ) {
        case PaulaErr_Warning :
        case PaulaErr_Error :
            ErrorString = PaulaDrv_ErrorString( ErrorCode );
            break;

        case PaulaErr_Ok :
            ErrorString = "Paula Audio ok.";
            break;

        case PaulaErr_Uninitialised:
            ErrorString = "Paula Audio uninitialised.";
            break;

        case PaulaErr_OpenDevice:
            ErrorString = "Paula Audio: could not open " AUDIONAME ".";
            break;

        case PaulaErr_IORequest:
            ErrorString = "Paula Audio: could not create the IO request.";
            break;

        case PaulaErr_MsgPort:
            ErrorString = "Paula Audio: could not create the Message Port.";
            break;

        case PaulaErr_ChipMem:
            ErrorString = "Paula Audio: could not allocate Chip RAM for the audio buffers.";
            break;

        case PaulaErr_NewGen:
            ErrorString = "Paula Audio: new generation systems are not supported.";
            break;

        default:
            ErrorString = "Unknown Paula Audio error code.";
            break;
    }

    return ErrorString;
}

int PaulaDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata)
{
	if (Initialised) {
		PaulaDrv_PCM_Shutdown();
	}

	if (((struct Library *)SysBase)->lib_Version >= 50 && (FindResident((STRPTR)"elf.library") || FindResident((STRPTR)"MorphOS"))) {
		// AmigaOS4 or MorphOS
		ErrorCode = PaulaErr_NewGen;
		return PaulaErr_Error;
	}

	if ((audioMP = CreateMsgPort())) {
		if ((audioIO = (struct IOAudio *)CreateIORequest(audioMP, sizeof(struct IOAudio)))) {
			UBYTE whichannel[] = {3};
			audioIO->ioa_Request.io_Message.mn_Node.ln_Pri = 127; // no stealing
			audioIO->ioa_Request.io_Command = ADCMD_ALLOCATE;
			audioIO->ioa_Request.io_Flags = ADIOF_NOWAIT;
			audioIO->ioa_AllocKey = 0;
			audioIO->ioa_Data = whichannel;
			audioIO->ioa_Length = sizeof(whichannel);
			if (!OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)audioIO, 0)) {
				if (*mixrate > 22050) {
					// we could go up to 28 KHz, but better stick to multiples of 11025
					*mixrate = 22050;
				}
				if (GfxBase->DisplayFlags & PAL) {
					period = 3546895 / *mixrate;
				} else {
					period = 3579545 / *mixrate;
				}

				if (*numchannels > 2) {
					*numchannels = 2;
				}
				channels = *numchannels;

				*samplebits = 8;

				Initialised = 1;
				return PaulaErr_Ok;
			} else {
				ErrorCode = PaulaErr_OpenDevice;
			}
		} else {
			ErrorCode = PaulaErr_IORequest;
		}
	} else {
		ErrorCode = PaulaErr_MsgPort;
	}

	PaulaDrv_PCM_Shutdown();

	return PaulaErr_Error;
}

void PaulaDrv_PCM_Shutdown(void)
{
	Initialised = 0;

	if (audioIO) {
		CloseDevice((struct IORequest *)audioIO);
		DeleteIORequest((struct IORequest *)audioIO);
		audioIO = NULL;
	}

	if (audioMP) {
		DeleteMsgPort(audioMP);
		audioMP = NULL;
	}
}

int PaulaDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
						int NumDivisions, void ( *CallBackFunc )( void ) )
{
	if (!Initialised) {
		ErrorCode = PaulaErr_Uninitialised;
		return PaulaErr_Error;
	}

    if (Playing) {
        PaulaDrv_PCM_StopPlayback();
    }

    MixBuffer = BufferStart;
    MixBufferSize = BufferSize;
    MixBufferCount = NumDivisions;
    MixBufferCurrent = 0;
    MixBufferUsed = 0;
    MixCallBack = CallBackFunc;
    
    // prime the buffer
    MixCallBack();

	// how many bytes MultiVoc can mix per channel
	channelSize = BufferSize / channels;
	//printf("MixBufferSize %d MixBufferCount %d channelSize %d\n", MixBufferSize, MixBufferCount, channelSize);

	// hardware channels are double buffered
	chipBuffer = AllocVec(BufferSize * 2, MEMF_CHIP|MEMF_CLEAR|MEMF_PUBLIC);

	if (chipBuffer) {
		// disable the DMA and interrupts
		custom.dmacon = DMAF_AUD0|DMAF_AUD1;
		custom.intena = INTF_AUD0;
		custom.intreq = INTF_AUD0;

		oldInt = SetIntVector(INTB_AUD0, &AudioInt);

		// left channel
		custom.aud[0].ac_len = channelSize / sizeof(UWORD);
		custom.aud[0].ac_per = period;
		custom.aud[0].ac_vol = 64;
		custom.aud[0].ac_ptr = (UWORD *)chipBuffer;

		// right channel
		custom.aud[1].ac_len = channelSize / sizeof(UWORD);
		custom.aud[1].ac_per = period;
		custom.aud[1].ac_vol = 64;
		custom.aud[1].ac_ptr = (UWORD *)chipBuffer + ((channels > 1) ? channelSize : 0);

		// enable the DMA and interrupts
		custom.intena = INTF_SETCLR|INTF_INTEN|INTF_AUD0;
		custom.dmacon = DMAF_SETCLR|DMAF_AUD0|DMAF_AUD1;

		// turn off the low-pass filter
		oldFilter = ciaa->ciapra & CIAF_LED;
		ciaa->ciapra |= CIAF_LED;

		actsound = 0;
		Playing = 1;

		return PaulaErr_Ok;
	} else {
		ErrorCode = PaulaErr_ChipMem;
	}

	PaulaDrv_PCM_StopPlayback();

	return PaulaErr_Error;
}

void PaulaDrv_PCM_StopPlayback(void)
{
    if (!Initialised || !Playing) {
        return;
    }

	if (!oldFilter) {
		ciaa->ciapra &= ~CIAF_LED;
	}

	if (oldInt) {
		custom.dmacon = DMAF_AUD0|DMAF_AUD1;
		custom.intena = INTF_AUD0;
		SetIntVector(INTB_AUD0, oldInt);
		oldInt = NULL;
	}

	if (chipBuffer) {
		FreeVec(chipBuffer);
		chipBuffer = NULL;
	}

	Playing = 0;
}

void PaulaDrv_PCM_Lock(void)
{
}

void PaulaDrv_PCM_Unlock(void)
{
}
