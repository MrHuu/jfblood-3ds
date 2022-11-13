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
#include "driver_cdplayer.h"

#include <clib/alib_protos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <libraries/cdplayer.h>
#include <proto/cdplayer.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>

static struct IOStdReq *cdRequest = NULL;
static struct MsgPort *cdPort = NULL;
static BYTE cdDevice = -1;
static struct Process *cdParentProc = NULL;
static struct Process *cdWatchProc = NULL;
static struct SignalSemaphore cdSemaphore;
static int cdPaused = 0;
static int cdLoop = 0;
static int cdPlayTrack = 0;

struct Library *CDPlayerBase = NULL;

enum {
    CDPlayerErr_Warning = -2,
    CDPlayerErr_Error   = -1,
    CDPlayerErr_Ok      = 0,
    CDPlayerErr_Uninitialised,
    CDPlayerErr_OpenDevice,
    CDPlayerErr_IORequest,
    CDPlayerErr_MsgPort,
    CDPlayerErr_OpenLibrary,
    CDPlayerErr_CDCreateThread,
    CDPlayerErr_CDCannotPlayTrack,
};

static int ErrorCode = CDPlayerErr_Ok;

int CDPlayerDrv_GetError(void)
{
    return ErrorCode;
}

const char *CDPlayerDrv_ErrorString( int ErrorNumber )
{
    const char *ErrorString;

    switch( ErrorNumber ) {
        case CDPlayerErr_Warning :
        case CDPlayerErr_Error :
            ErrorString = CDPlayerDrv_ErrorString( ErrorCode );
            break;

        case CDPlayerErr_Ok :
            ErrorString = "CD Player ok.";
            break;

        case CDPlayerErr_Uninitialised:
            ErrorString = "CD Player uninitialised.";
            break;

        case CDPlayerErr_OpenDevice:
            ErrorString = "CD Player: could not open the CD device.";
            break;

        case CDPlayerErr_IORequest:
            ErrorString = "CD Player: could not create the IO request.";
            break;

        case CDPlayerErr_MsgPort:
            ErrorString = "CD Player: could not create the Message Port.";
            break;

        case CDPlayerErr_OpenLibrary:
            ErrorString = "CD Player: could not open " CDPLAYERNAME ".";
            break;
        
        case CDPlayerErr_CDCreateThread:
            ErrorString = "CD Player: could not create looped CD playback thread.";
            break;

        case CDPlayerErr_CDCannotPlayTrack:
            ErrorString = "CD Player: cannot play the requested track.";
            break;

        default:
            ErrorString = "Unknown CD Player error code.";
            break;
    }

    return ErrorString;
}

int CDPlayerDrv_CD_Init(void)
{
    if ((CDPlayerBase = OpenLibrary((STRPTR)CDPLAYERNAME, CDPLAYERVERSION))) {
        if ((cdPort = CreateMsgPort())) {
            if ((cdRequest = (struct IOStdReq *)CreateIORequest(cdPort, sizeof(struct IOStdReq)))) {
                struct DosList *dol;
                char *dol_Name = NULL;
                char *fssm_Device = NULL;
                ULONG fssm_Unit = 0;
                struct CD_TOC table = {0};

                // look for the device
                cdDevice = -1;
                if ((dol = LockDosList(LDF_DEVICES | LDF_READ))) {
                    while ((dol = NextDosEntry(dol, LDF_DEVICES))) {
                        dol_Name = BADDR(dol->dol_Name) + 1;
                        // is this a CD device?
                        if (dol_Name[0] == 'C' && dol_Name[1] == 'D' && dol_Name[2] >= '0' && dol_Name[2] <= '9') {
                            struct FileSysStartupMsg *fssm = (struct FileSysStartupMsg *)BADDR(dol->dol_misc.dol_handler.dol_Startup);
                            if (fssm && TypeOfMem(fssm)) {
                                fssm_Device = BADDR(fssm->fssm_Device) + 1;
                                fssm_Unit = fssm->fssm_Unit;
                                if (!(cdDevice = OpenDevice((STRPTR)fssm_Device, fssm_Unit, (struct IORequest *)cdRequest, 0))) {
                                    if (!CDReadTOC(&table, cdRequest) && table.cdc_NumTracks > 0) {
                                        break;
                                    } else {
                                        CloseDevice((struct IORequest *)cdRequest);
                                        cdDevice = -1;
                                    }
                                }
                            }
                        }
                    }
                    UnLockDosList(LDF_DEVICES | LDF_READ);
                }
                if (!cdDevice) {
                    printf("CDPlayer: %s: device %s unit %lu tracks %d\n", dol_Name, fssm_Device, fssm_Unit, table.cdc_NumTracks);
                    /*for (int i = 0; i < table.cdc_NumTracks; i++) {
                        printf("CDPlayer: track %d - %s, %02ld:%02ld\n",
                            i,
                            table.cdc_Flags[i] ? "data " : "audio",
                            BASE2MIN(table.cdc_Addr[i+1] - table.cdc_Addr[i]), BASE2SEC(table.cdc_Addr[i+1] - table.cdc_Addr[i]));
                    }*/
                    InitSemaphore(&cdSemaphore);
                    return CDPlayerErr_Ok;
                } else {
                    ErrorCode = CDPlayerErr_OpenDevice;
                }
            } else {
                ErrorCode = CDPlayerErr_IORequest;
            }
        } else {
            ErrorCode = CDPlayerErr_MsgPort;
        }
    } else {
        ErrorCode = CDPlayerErr_OpenLibrary;
    }

    CDPlayerDrv_CD_Shutdown();

    return CDPlayerErr_Error;

}

void CDPlayerDrv_CD_Shutdown(void)
{
    CDPlayerDrv_CD_Stop();

    if (cdRequest) {
        if (!cdDevice) {
            CloseDevice((struct IORequest *)cdRequest);
            cdDevice = -1;
        }
        DeleteIORequest(cdRequest);
        cdRequest = NULL;
    }
    if (cdPort) {
        DeleteMsgPort(cdPort);
        cdPort = NULL;
    }
    if (CDPlayerBase) {
        CloseLibrary(CDPlayerBase);
        CDPlayerBase = NULL;
    }
}

static void cdWatchThread(void)
{
    BOOL status;
    struct MsgPort replyPort;
    
    memset(&replyPort, 0, sizeof(replyPort));
    replyPort.mp_Node.ln_Type = NT_MSGPORT;
    replyPort.mp_Flags = PA_SIGNAL;
    replyPort.mp_SigTask = FindTask(NULL);
    SetSignal(SIGBREAKF_CTRL_D, 0);
    replyPort.mp_SigBit = SIGBREAKB_CTRL_D;
    NewList(&replyPort.mp_MsgList);
    
    while (!(SetSignal(0, 0) & SIGBREAKF_CTRL_C)) {
        Delay(50);
        if (!cdLoop || cdPaused) {
            continue;
        }
        
        if (AttemptSemaphore(&cdSemaphore)) {
            cdRequest->io_Message.mn_ReplyPort = &replyPort;
            status = CDActive(cdRequest);
            cdRequest->io_Message.mn_ReplyPort = cdPort;
            ReleaseSemaphore(&cdSemaphore);
        } else {
            // check it the next iteration
            status = TRUE;
        }
        if (!status) {
            // TODO check the return value of CDPlay?
            ObtainSemaphore(&cdSemaphore);
            cdRequest->io_Message.mn_ReplyPort = &replyPort;
            CDPlay(cdPlayTrack, cdPlayTrack, cdRequest);
            cdRequest->io_Message.mn_ReplyPort = cdPort;
            ReleaseSemaphore(&cdSemaphore);
        }
    }

    Forbid();
    Signal((struct Task *)cdParentProc, SIGBREAKF_CTRL_E);
}

int CDPlayerDrv_CD_Play(int track, int loop)
{
    if (cdDevice) {
        ErrorCode = CDPlayerErr_Uninitialised;
        return CDPlayerErr_Error;
    }
    
    CDPlayerDrv_CD_Stop();
    
    if (CDPlay(track, track, cdRequest)) {
        ErrorCode = CDPlayerErr_CDCannotPlayTrack;
        return CDPlayerErr_Error;
    }
    
    cdLoop = loop;
    cdPlayTrack = track;
    cdPaused = 0;
    
    if (loop) {
        cdParentProc = (struct Process *)FindTask(NULL);
        cdWatchProc = CreateNewProcTags(NP_Name, (Tag)"JFAudioLib CD Watch Thread", NP_Entry, (Tag)cdWatchThread, TAG_END);
        if (!cdWatchProc) {
            cdLoop = 0;
            ErrorCode = CDPlayerErr_CDCreateThread;
            return CDPlayerErr_Warning;  // play, but we won't be looping
        }
    }
    
    return CDPlayerErr_Ok;
}

void CDPlayerDrv_CD_Stop(void)
{
    if (cdDevice) {
        return;
    }
    
    if (cdWatchProc) {
        Signal((struct Task *)cdWatchProc, SIGBREAKF_CTRL_C);
        Wait(SIGBREAKF_CTRL_E);
        cdWatchProc = NULL;
    }

    cdLoop = 0;
    cdPlayTrack = 0;
    cdPaused = 0;

    CDStop(cdRequest);
}

void CDPlayerDrv_CD_Pause(int pauseon)
{
    if (cdDevice) {
        return;
    }

    if (cdPaused == pauseon) {
        return;
    }
    
    ObtainSemaphore(&cdSemaphore);
    CDResume(pauseon, cdRequest);
    ReleaseSemaphore(&cdSemaphore);

    cdPaused = pauseon;
}

int CDPlayerDrv_CD_IsPlaying(void)
{
    BOOL status;

    if (cdDevice) {
        return 0;
    }

    ObtainSemaphore(&cdSemaphore);
    status = CDActive(cdRequest);
    ReleaseSemaphore(&cdSemaphore);

    return status;
}

void CDPlayerDrv_CD_SetVolume(int volume)
{
    struct CD_Volume vol;

    if (cdDevice) {
        return;
    }

    vol.cdv_Chan0 = vol.cdv_Chan1 = volume;
    ObtainSemaphore(&cdSemaphore);
    CDSetVolume(&vol, cdRequest);
    ReleaseSemaphore(&cdSemaphore);
}
