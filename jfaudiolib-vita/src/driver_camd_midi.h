/*
 Copyright (C) 2021 Mathias Heyer <sonode@gmx.de>

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
#ifndef DRIVER_CAMD_MIDI_H
#define DRIVER_CAMD_MIDI_H

#include "midifuncs.h"

int CamdDrv_GetError(void);
const char *CamdDrv_ErrorString( int ErrorNumber );

int  CamdDrv_MIDI_Init(midifuncs *, const char *);
void CamdDrv_MIDI_Shutdown(void);
int  CamdDrv_MIDI_StartPlayback(void (*service)(void));
void CamdDrv_MIDI_HaltPlayback(void);
void CamdDrv_MIDI_SetTempo(int tempo, int division);
void CamdDrv_MIDI_Lock(void);
void CamdDrv_MIDI_Unlock(void);
#endif // DRIVER_CAMD_MIDI_H
