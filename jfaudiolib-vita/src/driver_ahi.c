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
 * AHI output driver for MultiVoc
 */

#include <proto/ahi.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <devices/ahi.h>

#include <SDI_compiler.h>
#include <SDI_hook.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver_ahi.h"

enum {
    AHIErr_Warning = -2,
    AHIErr_Error   = -1,
    AHIErr_Ok      = 0,
    AHIErr_Uninitialised,
    AHIErr_InitSubSystem,
    AHIErr_OpenAudio,
};

static int ErrorCode = AHIErr_Ok;
static int Initialised = 0;
static int Playing = 0;
static int actsound = 0;
static struct AHISampleInfo sample[2];
struct Library *AHIBase;
static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIio = NULL;
static BYTE AHIDevice = -1;
static struct AHIAudioCtrl *actrl = NULL;

static char *MixBuffer = 0;
static int MixBufferSize = 0;
static int MixBufferCount = 0;
static int MixBufferCurrent = 0;
static int MixBufferUsed = 0;
static void ( *MixCallBack )( void ) = 0;

static void FillBufferPortion(char * ptr, int remaining)
{
	int len;
	char *sptr;

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
			
			memcpy(ptr, sptr, len);
			
			ptr += len;
			MixBufferUsed += len;
			remaining -= len;
		}
	}
}

HOOKPROTO(fillData, ULONG, struct AHIAudioCtrl *actrl, struct AHISoundMessage *smsg)
{
	char *ptr;
	int remaining;

	actsound ^= 1;
	ptr = (char *)sample[actsound].ahisi_Address;
	remaining = sample[actsound].ahisi_Length * (int)actrl->ahiac_UserData;

	FillBufferPortion(ptr, remaining);
	AHI_SetSound(0, actsound, 0, 0, actrl, 0);

	return 0;
}
MakeHook(SoundHook, fillData);

int AHIDrv_GetError(void)
{
    return ErrorCode;
}

const char *AHIDrv_ErrorString( int ErrorNumber )
{
    const char *ErrorString;

    switch( ErrorNumber ) {
        case AHIErr_Warning :
        case AHIErr_Error :
            ErrorString = AHIDrv_ErrorString( ErrorCode );
            break;

        case AHIErr_Ok :
            ErrorString = "AHI Audio ok.";
            break;

        case AHIErr_Uninitialised:
            ErrorString = "AHI Audio uninitialised.";
            break;

        case AHIErr_InitSubSystem:
            ErrorString = "AHI Audio: error in Init or InitSubSystem.";
            break;

        case AHIErr_OpenAudio:
            ErrorString = "AHI Audio: error in OpenAudio.";
            break;

        default:
            ErrorString = "Unknown AHI Audio error code.";
            break;
    }

    return ErrorString;
}

int AHIDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata)
{
	if (Initialised) {
		AHIDrv_PCM_Shutdown();
	}

	if ((AHImp = CreateMsgPort())) {
		if ((AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest)))) {
			AHIio->ahir_Version = 4;
			if (!(AHIDevice = OpenDevice((STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)AHIio, 0))) {
				ULONG type;

				AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;

				if (*numchannels == 1) {
					if (*samplebits == 8)
						type = AHIST_M8S;
					else
						type = AHIST_M16S;
				} else {
					if (*samplebits == 8)
						type = AHIST_S8S;
					else
						type = AHIST_S16S;
				}
				sample[0].ahisi_Type = sample[1].ahisi_Type = type;
				actrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
					AHIA_MixFreq, *mixrate,
					AHIA_Channels, 1,
					AHIA_Sounds, 2,
					AHIA_SoundFunc, (IPTR)&SoundHook,
					AHIA_UserData, AHI_SampleFrameSize(type),
					TAG_DONE);

				if (actrl) {
					ULONG mixfreq;
					AHI_ControlAudio(actrl, AHIC_MixFreq_Query, (IPTR)&mixfreq, TAG_DONE);
					*mixrate = mixfreq;
					Initialised = 1;

					return AHIErr_Ok;
				}
			}
		}
	}

	AHIDrv_PCM_Shutdown();

	return AHIErr_Error;
}

void AHIDrv_PCM_Shutdown(void)
{
	Initialised = 0;

	if (actrl) {
		AHI_FreeAudio(actrl);
		actrl = NULL;
	}

	if (!AHIDevice) {
		CloseDevice((struct IORequest *)AHIio);
		AHIDevice = -1;
		AHIBase = NULL;
	}

	if (AHIio) {
		DeleteIORequest((struct IORequest *)AHIio);
		AHIio = NULL;
	}

	if (AHImp) {
		DeleteMsgPort(AHImp);
		AHImp = NULL;
	}
}

int AHIDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
						int NumDivisions, void ( *CallBackFunc )( void ) )
{
	if (!Initialised) {
		ErrorCode = AHIErr_Uninitialised;
		return AHIErr_Error;
	}

    if (Playing) {
        AHIDrv_PCM_StopPlayback();
    }

    MixBuffer = BufferStart;
    MixBufferSize = BufferSize;
    MixBufferCount = NumDivisions;
    MixBufferCurrent = 0;
    MixBufferUsed = 0;
    MixCallBack = CallBackFunc;
    
    // prime the buffer
    MixCallBack();

	ULONG buflen = BufferSize / NumDivisions;
	ULONG bufsize = buflen * (ULONG)actrl->ahiac_UserData;

	sample[0].ahisi_Address = AllocVec(bufsize, MEMF_CLEAR);
	sample[0].ahisi_Length = buflen;

	sample[1].ahisi_Address = AllocVec(bufsize, MEMF_CLEAR);
	sample[1].ahisi_Length = buflen;

	if (sample[0].ahisi_Address && sample[1].ahisi_Address) {
		if (!AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample[0], actrl) &&
			!AHI_LoadSound(1, AHIST_DYNAMICSAMPLE, &sample[1], actrl)) {
			if (!(AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_DONE))) {
				ULONG mixfreq;

				actsound = 0;
				AHI_ControlAudio(actrl, AHIC_MixFreq_Query, (IPTR)&mixfreq, TAG_DONE);
				AHI_Play(actrl,
					AHIP_BeginChannel, 0,
					AHIP_Freq, mixfreq,
					AHIP_Vol, 0x10000,
					AHIP_Pan, 0x8000,
					AHIP_Sound, actsound,
					AHIP_EndChannel, 0,
					TAG_END);

				Playing = 1;

				return AHIErr_Ok;
			}
		}
	}

	AHIDrv_PCM_StopPlayback();

	ErrorCode = AHIErr_Uninitialised; // TODO error code
	return AHIErr_Error;
}

void AHIDrv_PCM_StopPlayback(void)
{
    if (!Initialised || !Playing) {
        return;
    }

	AHI_ControlAudio(actrl, AHIC_Play, FALSE, TAG_END);

	if (sample[0].ahisi_Address) {
		FreeVec(sample[0].ahisi_Address);
		sample[0].ahisi_Address = NULL;
	}

	if (sample[1].ahisi_Address) {
		FreeVec(sample[1].ahisi_Address);
		sample[1].ahisi_Address = NULL;
	}

	Playing = 0;
}

void AHIDrv_PCM_Lock(void)
{
}

void AHIDrv_PCM_Unlock(void)
{
}
