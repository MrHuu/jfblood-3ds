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
 * MPEG source support for MultiVoc
 */

#ifdef HAVE_MPEG

#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>

#include <proto/mhi.h>
#include <libraries/mhi.h>
#undef TRUE
#undef FALSE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
# include <unistd.h>
#endif
#include <errno.h>
#include "pitch.h"
#include "multivoc.h"
#include "_multivc.h"

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

#define BUFSIZE (0x8000)

typedef struct
{
   struct Library *libbase;
   struct Task *thread;
   struct Task *parent;
   UWORD volume;
} mpeg_data;


/*---------------------------------------------------------------------
Function: MV_GetNextMPEGBlock

Controls playback of MPEG data
---------------------------------------------------------------------*/

static playbackstatus MV_GetNextMPEGBlock
(
 VoiceNode *voice
 )

{
   return( NoMoreData ); // prevent the mixer from running
}


/*---------------------------------------------------------------------
Function: MV_PlayMPEG

Begin playback of sound data with the given sound levels and
priority.
---------------------------------------------------------------------*/

int MV_PlayMPEG
(
 char *ptr,
 unsigned int ptrlength,
 int   pitchoffset,
 int   vol,
 int   left,
 int   right,
 int   priority,
 unsigned int callbackval
 )

{
   int status;
   
   status = MV_PlayLoopedMPEG( ptr, ptrlength, -1, -1, pitchoffset, vol, left, right,
                                 priority, callbackval );
   
   return( status );
}


/*---------------------------------------------------------------------
Function: MV_PlayLoopedMPEG

Begin playback of sound data with the given sound levels and
priority.
---------------------------------------------------------------------*/

static void bufferUpdate(void)
{
   struct Process *me = (struct Process *)FindTask(NULL);
   VoiceNode *voice = (VoiceNode *)me->pr_ExitData;
   me->pr_ExitData = 0;
   mpeg_data * md = (mpeg_data *) voice->extra;
   struct Library *MHIBase = md->libbase;
   APTR handle;
   int i;

   //printf("%s voice %p started thread %p\n", __FUNCTION__, voice, me);
   if ((handle = MHIAllocDecoder((struct Task *)me, SIGBREAKF_CTRL_F))) {
      //printf("%s voice %p allocated MHI handle %p\n", __FUNCTION__, voice, handle);
      // Fill two buffers and start playing
      for (i = 0; i < 2; i++) {
         int length          = min( voice->BlockLength, BUFSIZE );
         MHIQueueBuffer(handle, voice->NextBlock, length);
         voice->NextBlock   += length;
         voice->BlockLength -= length;
      }
      MHIPlay(handle);
      do {
         ULONG signals = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);

         if (signals & SIGBREAKF_CTRL_C) {
            break;
         }

         if (signals & SIGBREAKF_CTRL_D) {
            //printf("%s voice %p handle %p volume %d\n", __FUNCTION__, voice, handle, md->volume);
            MHISetParam(handle, MHIP_VOLUME, md->volume);
         }

         if (signals & SIGBREAKF_CTRL_E) {
            // TODO pause support
         }

         if (signals & SIGBREAKF_CTRL_F) {
            // free up the empty buffers
            while (MHIGetEmpty(handle)) ;
            // queue the next buffer
            if ( voice->BlockLength <= 0 )
               {
               if ( voice->LoopStart == NULL )
                  {
                  voice->Playing = FALSE;
                  break;
                  }

              voice->BlockLength = voice->LoopSize;
              voice->NextBlock   = voice->LoopStart;
              }

            int length          = min( voice->BlockLength, BUFSIZE );
            MHIQueueBuffer(handle, voice->NextBlock, length);
            voice->NextBlock   += length;
            voice->BlockLength -= length;

            // Restart if needed
            if (MHIGetStatus(handle) == MHIF_OUT_OF_DATA) {
               MHIPlay(handle);
            }
         }
      } while (TRUE);
      MHIStop(handle);
      MHIFreeDecoder(handle);
   }

   Forbid();
   Signal(md->parent, SIGBREAKF_CTRL_E);
}

int MV_PlayLoopedMPEG
(
 char *ptr,
 unsigned int ptrlength,
 int   loopstart,
 int   loopend,
 int   pitchoffset,
 int   vol,
 int   left,
 int   right,
 int   priority,
 unsigned int callbackval
 )

{
   VoiceNode   *voice;
   mpeg_data * md = NULL;
   unsigned char *uptr = (unsigned char *)ptr;
   
   if ( !MV_Installed ) {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
   }
   
   // RIFF WAVE
   if (!memcmp("RIFF", uptr, 4)) {
      uptr += 12;
      while (memcmp("data", uptr, 4)) {
         uptr += 4;
         int size = uptr[3] << 24 | uptr[2] << 16 | uptr[1] << 8 | uptr[0];
         uptr += 4 + size;
      }
      uptr += 4;
      ptrlength = uptr[3] << 24 | uptr[2] << 16 | uptr[1] << 8 | uptr[0];
      uptr += 4;
   }
   
   // ID3v2
   if (!memcmp("ID3", uptr, 3)) {
      int size = 10 + (uptr[6] << 21 | uptr[7] << 14 | uptr[8] << 7 | uptr[9]);
      uptr += size;
      ptrlength -= size;
   }

   // ID3v1
   if (!memcmp("TAG", uptr + ptrlength - 128, 3)) {
      ptrlength -= 128;
   }

   // Enhanced TAG
   if (!memcmp("TAG+", uptr + ptrlength - 227, 4)) {
      ptrlength -= 227;
   }

   // MPEG sync word
   if (uptr[0] != 0xFF && (uptr[1] & 0xF0) != 0xF0) {
      MV_SetErrorCode( MV_InvalidMPEGFile );
      return MV_Error;
   }
   
   md = (mpeg_data *) malloc( sizeof(mpeg_data) );
   if (!md) {
      MV_SetErrorCode( MV_InvalidMPEGFile );
      return MV_Error;
   }

   memset(md, 0, sizeof(mpeg_data));
   md->parent = FindTask(NULL);

   struct AnchorPath apath;
   memset(&apath, 0, sizeof(apath));
   LONG error = MatchFirst((STRPTR)"LIBS:MHI/mhi#?.library", &apath);
   while (!error) {
      struct Library *MHIBase;
      char buffer[256];
      NameFromLock(apath.ap_Current->an_Lock, (STRPTR)buffer, sizeof(buffer));
      AddPart((STRPTR)buffer, (STRPTR)apath.ap_Info.fib_FileName, sizeof(buffer));
      //printf("%s trying to open %s\n", __FUNCTION__, buffer);
      if ((MHIBase = OpenLibrary((STRPTR)buffer, 0))) {
         //printf("%s trying to allocate a handle\n", __FUNCTION__);
         APTR handle;
         if ((handle = MHIAllocDecoder(md->parent, SIGBREAKF_CTRL_F))) {
            MHIFreeDecoder(handle);
            md->libbase = MHIBase;
            //printf("%s success base %p\n", __FUNCTION__, MHIBase);
            break;
         }
         CloseLibrary(MHIBase);
      }
      error = MatchNext(&apath);
   }
   MatchEnd(&apath);

   if (!md->libbase) {
      free(md);
      MV_SetErrorCode( MV_DriverError );
      return( MV_Error );
   }
   
   // Request a voice from the voice pool
   voice = MV_AllocVoice( priority );
   if ( voice == NULL ) {
      CloseLibrary(md->libbase);
      free(md);
      MV_SetErrorCode( MV_NoVoices );
      return( MV_Error );
   }
   
   voice->wavetype    = MPEG;
   voice->bits        = 16;
   voice->channels    = 2;
   voice->extra       = (void *) md;
   voice->GetSound    = MV_GetNextMPEGBlock;
   voice->NextBlock   = (char *) uptr;
   voice->DemandFeed  = NULL;
   voice->LoopCount   = 0;
   voice->BlockLength = ptrlength;
   voice->PitchScale  = 0;
   voice->length      = 0; // prevent the mixer from running
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;
   voice->LoopStart   = (char *)(loopstart >= 0 ? uptr : NULL);
   voice->LoopEnd     = 0;
   voice->LoopSize    = ptrlength;
   voice->Playing     = TRUE;
   voice->Paused      = FALSE;
   
   voice->SamplingRate = 44010;
   voice->RateScale    = 0;
   voice->FixedPointBufferSize = 0;
   voice->mix          = NULL;

   MV_SetVoiceVolume( voice, vol, left, right );
   MV_PlayVoice( voice );
   
   md->thread = (struct Task *)CreateNewProcTags(
       NP_Name, (Tag)"JFAudioLib MHI thread",
       NP_Priority, 5,
       NP_Entry, (Tag)bufferUpdate,
       NP_ExitData, (Tag)voice,
       TAG_END);
   //printf("%s started thread %p for voice %p\n", __FUNCTION__, md->thread, voice);

   if (!md->thread) {
      // TODO we should call MV_StopVoice(voice) here, but it's a static function
   }
   
   return( voice->handle );
}


void MV_ReleaseMPEGVoice( VoiceNode * voice )
{
   mpeg_data * md = (mpeg_data *) voice->extra;
   
   if (voice->wavetype != MPEG) {
      return;
   }
   
   if (md->thread) {
      //printf("%s closing thread %p for voice %p\n", __FUNCTION__, md->thread, voice);
      Signal(md->thread, SIGBREAKF_CTRL_C);
      Wait(SIGBREAKF_CTRL_E);
   }
   //printf("%s closing base %p\n", __FUNCTION__, md->libbase);
   CloseLibrary(md->libbase);
   free(md);
   
   voice->extra = 0;
}

void MV_SetMPEGVolume( VoiceNode *voice, int vol, int left, int right )
{
   mpeg_data * md = (mpeg_data *) voice->extra;
   
   if (voice->wavetype != MPEG) {
      return;
   }
   
   md->volume = vol * 100 / MV_MaxTotalVolume;
   //printf("%s voice %p volume %d -> %d\n", __FUNCTION__, voice, vol, md->volume);
   if (md->thread) {
       //printf("%s signaling thread %p to wake up\n", __FUNCTION__, md->thread);
       Signal(md->thread, SIGBREAKF_CTRL_D);
   }
   // TODO panning
}

#endif //HAVE_MPEG
