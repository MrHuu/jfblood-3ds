// Amiga interface layer
// for the Build Engine
// by Szilard Biro (col.lawrence@gmail.com)
//


#include <devices/input.h>
#if defined __AROS__
#include <devices/rawkeycodes.h>
#define NM_WHEEL_UP RAWKEY_NM_WHEEL_UP
#define NM_WHEEL_DOWN RAWKEY_NM_WHEEL_DOWN
#define NM_BUTTON_FOURTH RAWKEY_NM_BUTTON_FOURTH
#elif !defined __MORPHOS__
#include <newmouse.h>
#endif
#include <libraries/lowlevel.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/videocontrol.h>
#include <workbench/startup.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/keymap.h>
#include <proto/lowlevel.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/graphics.h>
#include <proto/icon.h>

#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>

#include <devices/gameport.h>
#include <psxport.h>

#include <SDI_compiler.h>
#include <SDI_interrupt.h>
#undef UNUSED

#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include "build.h"
//#include "amilayer.h"
#include "baselayer.h"
#include "cache1d.h"
#include "pragmas.h"
#include "a.h"
#include "osd.h"
#include "glbuild.h"

int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *UNUSED(s)) { return 0; }
int startwin_idle(void *s) { return 0; }
int startwin_settitle(const char *s) { s=s; return 0; }

int   _buildargc = 1;
const char **_buildargv = NULL;

char quitevent=0, appactive=1;

static char apptitle[256] = "Build Engine";
static char wintitle[256] = "";

// video
static struct Window *window = NULL;
static struct Screen *screen = NULL;
static unsigned char ppal[256 * 4];
static ULONG spal[1 + (256 * 3) + 1];
static int updatePalette = 0;
#ifndef __AROS__
struct Library *CyberGfxBase = NULL;
#else
struct Library *LowLevelBase;
#endif
static int use_c2p = 0;
static int use_wcp = 0;
static int currentBitMap;
static struct ScreenBuffer *sbuf[2];
static struct RastPort temprp;
static struct MsgPort *dispport;
static int safetochange;
static ULONG fsMonitorID = INVALID_ID;
static ULONG fsModeID = INVALID_ID;
static char wndPubScreen[32] = {"Workbench"};

static unsigned char *frame;
int xres=-1, yres=-1, bpp=0, fullscreen=0, bytesperline, imageSize;
intptr_t frameplace=0;
char modechange=1;
char offscreenrendering=0;
char videomodereset = 0;
extern int gammabrightness;
extern float curgamma;

// input
static struct MsgPort *inputPort;
static struct IOStdReq *inputReq;
static UWORD *pointermem;
static struct MsgPort *gameport_mp = NULL;
static struct IOStdReq *gameport_io = NULL;
static BOOL gameport_is_open = FALSE;
static struct InputEvent gameport_ie;
static BYTE gameport_ct;		/* controller type */
static BOOL analog_centered = FALSE;
static int analog_clx;
static int analog_cly;
static int analog_crx;
static int analog_cry;
struct GamePortTrigger gameport_gpt = {
	GPTF_UPKEYS | GPTF_DOWNKEYS,	/* gpt_Keys */
	0,				/* gpt_Timeout */
	1,				/* gpt_XDelta */
	1				/* gpt_YDelta */
};
int inputdevices=0;
char keystatus[256];
int keyfifo[KEYFIFOSIZ];
unsigned char keyasciififo[KEYFIFOSIZ];
int keyfifoplc, keyfifoend;
int keyasciififoplc, keyasciififoend;
int mousex=0,mousey=0,mouseb=0;
int joyaxis[8], joyb=0;
char joynumaxes=0, joynumbuttons=0;
static char mouseacquired=0,moustat=0;

void (*keypresscallback)(int,int) = 0;
void (*mousepresscallback)(int,int) = 0;
void (*joypresscallback)(int,int) = 0;

#ifdef __AROS__
// dummy
static void c2p_write_bm(WORD chunkyx, WORD chunkyy, WORD offsx, WORD offsy, APTR chunkyscreen, struct BitMap *bitmap) {}
#else
static ULONG oldFPCR = -1;
//extern ULONG ASM getfpcr(void);
//extern void ASM setfpcr(REG(d0,ULONG val));
extern void ASM c2p1x1_8_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
extern void ASM c2p1x1_8_c5_bm_040(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
typedef void ASM (*c2p_write_bm_func)(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
static c2p_write_bm_func c2p_write_bm;
#define TIMERINT
#endif

static void shutdownvideo(void);
static void constrainmouse(int a);

int wm_msgbox(const char *value, const char *fmt, ...)
{
	char buf[512];
	va_list va;

	if (!value) {
		value = apptitle;
	}

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

	if (IntuitionBase) {
		struct EasyStruct es;
		es.es_StructSize = sizeof(es);
		es.es_Flags = 0;
		es.es_Title = (STRPTR)value;
		es.es_TextFormat = (STRPTR)buf;
		es.es_GadgetFormat = (STRPTR)"OK";
		int wasacquired = mouseacquired;
		constrainmouse(0);
		EasyRequestArgs(/*window*/NULL, &es, NULL, NULL);
		constrainmouse(wasacquired);
	} else {
		puts(buf);
	}

	return 0;
}

int wm_ynbox(const char *value, const char *fmt, ...)
{
	char buf[512];
	int rv;
	va_list va;

	if (!value) {
		value = apptitle;
	}

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

	if (IntuitionBase) {
		struct EasyStruct es;
		es.es_StructSize = sizeof(es);
		es.es_Flags = 0;
		es.es_Title = (STRPTR)value;
		es.es_TextFormat = (STRPTR)buf;
		es.es_GadgetFormat = (STRPTR)"Yes|No";
		int wasacquired = mouseacquired;
		constrainmouse(0);
		rv = EasyRequestArgs(/*window*/NULL, &es, NULL, NULL);
		constrainmouse(wasacquired);
	} else {
		puts(buf);
		puts("   (assuming 'No')");
		rv = 0;
	}

	return rv;
}

int wm_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice)
{
	return -1;
}

void wm_setapptitle(const char *value)
{
	if (value) {
		Bstrncpy(apptitle, value, sizeof(apptitle)-1);
		apptitle[ sizeof(apptitle)-1 ] = 0;
	}
	//SetWindowTitles(window, (STRPTR)apptitle, (STRPTR)-1);
}

void wm_setwindowtitle(const char *value)
{
	if (value) {
		Bstrncpy(wintitle, value, sizeof(wintitle)-1);
		wintitle[ sizeof(wintitle)-1 ] = 0;
	}
	// TODO wm_setapptitle?
}


//
//
// ---------------------------------------
//
// System
//
// ---------------------------------------
//
//

int main(int argc, char *argv[])
{
	int r = 0;
	char *exename;
	struct DiskObject *appicon;
	BPTR progdir;

	_buildargc = argc;
	_buildargv = (const char **)argv;

	if (argc == 0) {
		struct WBStartup *startup = (struct WBStartup *)argv;
		exename = (char *)startup->sm_ArgList->wa_Name;
		progdir = startup->sm_ArgList->wa_Lock;
	} else {
		exename = argv[0];
		progdir = GetProgramDir();
	}

	if ((appicon = GetDiskObject((STRPTR)exename))) {
		char *value;

		if ((value = (char *)FindToolType(appicon->do_ToolTypes, (STRPTR)"FORCEMODE"))) {
			if (!strcmp(value, "NTSC"))
				fsMonitorID = NTSC_MONITOR_ID;
			else if (!strcmp(value, "PAL"))
				fsMonitorID = PAL_MONITOR_ID;
			else if (!strcmp(value, "MULTISCAN"))
				fsMonitorID = VGA_MONITOR_ID;
			else if (!strcmp(value, "EURO72"))
				fsMonitorID = EURO72_MONITOR_ID;
			else if (!strcmp(value, "EURO36"))
				fsMonitorID = EURO36_MONITOR_ID;
			else if (!strcmp(value, "SUPER72"))
				fsMonitorID = SUPER72_MONITOR_ID;
			else if (!strcmp(value, "DBLNTSC"))
				fsMonitorID = DBLNTSC_MONITOR_ID;
			else if (!strcmp(value, "DBLPAL"))
				fsMonitorID = DBLPAL_MONITOR_ID;
		}

		if ((value = (char *)FindToolType(appicon->do_ToolTypes, (STRPTR)"FORCEID"))) {
			int id;
			if (sscanf(value, "%x", &id) == 1) {
				fsModeID = id;
				fsMonitorID = INVALID_ID;
			}
		}

		if ((value = (char *)FindToolType(appicon->do_ToolTypes, (STRPTR)"PUBSCREEN"))) {
			strncpy(wndPubScreen, value, sizeof(wndPubScreen));
		}

		FreeDiskObject(appicon);
	}

	startwin_open();
	baselayer_init();

	r = app_main(_buildargc, (char const * const*)_buildargv);

	startwin_close();

	return r;
}


//
// initsystem() -- init Amiga systems
//
int initsystem(void)
{
	atexit(uninitsystem);

#ifndef __AROS__
	CyberGfxBase = OpenLibrary((STRPTR)"cybergraphics.library", 41);

	if (SysBase->AttnFlags & AFF_68040)
		c2p_write_bm = c2p1x1_8_c5_bm_040;
	else
		c2p_write_bm = c2p1x1_8_c5_bm;

	oldFPCR = getfpcr();
	setfpcr(0x00000020); // round toward minus infinity (RM)
#else
	LowLevelBase = OpenLibrary("lowlevel.libarary", 0);
#endif
	SetSignal(0, SIGBREAKF_CTRL_C);

	return 0;
}


//
// uninitsystem() -- uninit Amiga systems
//
void uninitsystem(void)
{
	uninitinput();
	uninitmouse();
	uninittimer();

	shutdownvideo();

#ifndef __AROS__
	if (CyberGfxBase) {
		CloseLibrary(CyberGfxBase);
		CyberGfxBase = NULL;
	}
	if (oldFPCR != (ULONG)-1)
	{
		setfpcr(oldFPCR);
		oldFPCR = (ULONG)-1;
	}
#else
	if (LowLevelBase) {
		CloseLibrary(LowLevelBase);
		LowLevelBase = NULL;
	}
#endif

}


//
// initputs() -- prints a string to the intitialization window
//
void initputs(const char *str)
{
	startwin_puts(str);
}


//
// debugprintf() -- prints a debug string to stderr
//
void debugprintf(const char *f, ...)
{
#ifdef DEBUGGINGAIDS
	va_list va;

	va_start(va,f);
	Bvfprintf(stderr, f, va);
	va_end(va);
#endif
}


//
//
// ---------------------------------------
//
// All things Input
//
// ---------------------------------------
//
//

static const char keyascii[] =
{
	'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',							/*  10 */
	'-', '=', '\\', 0, 0, 'q', 'w', 'e', 'r', 't',							/*  20 */
	'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, '2',						/*  30 */
	0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',							/*  40 */
	';', '\'', '#', 0, '4', '5', '6', '<', 'z', 'x',				/*  50 */
	'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '.',							/*  60 */
	0, '8', 0, 0x20, '\b', '\t', '\r', '\r', 0, 0,	/*  70 */
	0, 0, 0, '-', 0, 0, 0, 0, 0, 0,	/*  80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,						/*  90 */
	0, '/', '*', '+', 0, 0, 0, 0, 0, 0,					/* 100 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,									/* 110 */
};

static const char keyascii_shift[] =
{
	'~', '!', '\"', 0xA3/*Pound*/, '$', '%', '^', '&', '*', '(', ')',							/*  10 */
	'_', '+', '|', 0, 0, 'Q', 'W', 'E', 'R', 'T',							/*  20 */
	'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0, '2',						/*  30 */
	0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',							/*  40 */
	':', '\'', '@', 0, '4', '5', '6', '>', 'Z', 'X',				/*  50 */
	'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '.',							/*  60 */
	0, '8', 0, 0x20, '\b', '\t', '\r', '\r', 0, 0,	/*  70 */
	0, 0, 0, '-', 0, 0, 0, 0, 0, 0,	/*  80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,						/*  90 */
	0, '/', '*', '+', 0, 0, 0, 0, 0, 0,					/* 100 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,									/* 110 */
};

static const char *keynames[] =
{
	"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",							/*  10 */
	"-", "=", "\\", 0, "Insert", "q", "w", "e", "r", "t",							/*  20 */
	"y", "u", "i", "o", "p", "[", "]", 0, "End", "Keypad 2",						/*  30 */
	"Page Down", "a", "s", "d", "f", "g", "h", "j", "k", "l",							/*  40 */
	";", "\'", "#", 0, "Keypad 4", "Keypad 5", "Keypad 6", "<", "z", "x",				/*  50 */
	"c", "v", "b", "n", "m", ",", ".", "/", 0, "Delete",							/*  60 */
	"Home", "Keypad 8", "Page Up", "Space", "Backspace", "Tab", "Keypad Enter", "Enter", "Escape", "Delete",	/*  70 */
	0, 0, 0, "Keypad -", 0, "Up", "Down", "Right", "Left", "F1",	/*  80 */
	"F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "Num Lock",						/*  90 */
	"Scroll Lock", "Keypad /", "Keypad *", "Keypad +", "Help", "Left Shift", "Right Shift", "Caps Lock", "Ctrl", "Left Alt",					/* 100 */
	"Right Alt", 0, 0, 0, 0, 0, 0, 0, 0, 0,									/* 110 */
};

static const unsigned char keyconv[] =
{
	0x29, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb,							/*  10 */
	0xc, 0xd, 0x2b, 0/*U*/, 0xd2, 0x10, 0x11, 0x12, 0x13, 0x14,							/*  20 */
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0/*U*/, 0xcf , 0x50,						/*  30 */
	0xd1, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,							/*  40 */
	0x27, 0x28, 0/*#*/, 0/*U*/, 0x4b, 0x4c, 0x4d, 0/*<*/, 0x2c, 0x2d,				/*  50 */
	0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0/*U*/, 0xd3,							/*  60 */
	0xc7, 0x48, 0xc9, 0x39, 0xe, 0xf, 0x9c, 0x1c, 0x1, 0xd3,	/*  70 */
	0/*U*/, 0/*U*/, 0/*U*/, 0x4a, 0/*U*/, 0xc8, 0xd0, 0xcd, 0xcb, 0x3b,	/*  80 */
	0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45/*(*/,						/*  90 */
	0x46/*)*/, 0xb5, 0x37, 0x4e, 0xc5, 0x2a, 0x36, 0x3a, 0x1d, 0x38,					/* 100 */
	0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0,									/* 110 */
};
#define MAX_KEYCONV (sizeof keyconv / sizeof keyconv[0])

#define SetKey(key,state) { \
	keystatus[key] = state; \
		if (state) { \
	keyfifo[keyfifoend] = key; \
	keyfifo[(keyfifoend+1)&(KEYFIFOSIZ-1)] = state; \
	keyfifoend = ((keyfifoend+2)&(KEYFIFOSIZ-1)); \
		} \
}

#ifdef __AROS__
#ifndef HANDLERPROTO
// ABIv0 kludge
#define HANDLERPROTO(value, ret, obj, data) ret value(obj, data)
#undef MakeInterruptPri
#define MakeInterruptPri(value, func, title, isdata, pri) static struct Interrupt value = {{NULL, NULL, title, NT_INTERRUPT, pri}, (APTR)isdata, (void (*)())func}
#endif
#endif

HANDLERPROTO(InputHandlerFunc, struct InputEvent *, struct InputEvent *input_event, APTR data)
{
	struct InputEvent *ie;
	int code, press, j;

	// used for lag-free mouse input
	if (!mouseacquired)
		return input_event;

	if (!window || !(window->Flags & WFLG_WINDOWACTIVE) || (window->WScreen && window->WScreen != IntuitionBase->FirstScreen))
		return input_event;

	for (ie = input_event; ie; ie = ie->ie_NextEvent)
	{
		press = (ie->ie_Code & IECODE_UP_PREFIX) == 0;
		code = ie->ie_Code & ~IECODE_UP_PREFIX;

		if (ie->ie_Class == IECLASS_RAWMOUSE)
		{
			// mouse buttons
			if (code != IECODE_NOBUTTON)
			{
				int j = code - IECODE_LBUTTON;

				if (j >= 0)
				{
					if (press)
						mouseb |= (1<<j);
					else
						mouseb &= ~(1<<j);

					if (mousepresscallback) {
						mousepresscallback(j+1, press);
					}
				}

				ie->ie_Code = IECODE_NOBUTTON;
			}

			// mouse movement
			mousex += ie->ie_position.ie_xy.ie_x;
			mousey += ie->ie_position.ie_xy.ie_y;
			ie->ie_position.ie_xy.ie_x = 0;
			ie->ie_position.ie_xy.ie_y = 0;

		}
		//else if (ie->ie_Class == IECLASS_NEWMOUSE)
		else if (ie->ie_Class == IECLASS_RAWKEY && code >= NM_WHEEL_UP && code <= NM_BUTTON_FOURTH)
		{
			// mouse button 4, mouse wheel
			// up, down, left, right, fourth
			const int newmousebtn[] = {5, 4, -1, -1, 3};
			int j = newmousebtn[code - NM_WHEEL_UP];

			if (j >= 0)
			{
				if (press)
					mouseb |= (1<<j);
				else
					mouseb &= ~(1<<j);

				if (mousepresscallback) {
					if (j >= 4) { // wheel
						mousepresscallback(j+1, 1);
						mousepresscallback(j+1, 0);
					} else {
						mousepresscallback(j+1, press);
					}
				}
			}

			ie->ie_Code = IECODE_NOBUTTON;
		}
	}

	return input_event;
}
MakeInterruptPri(inputHandler, InputHandlerFunc, "Build input handler", NULL, 100);

static void RemoveInputHandler(void)
{
	if (inputReq) {
		inputReq->io_Data = &inputHandler;
		inputReq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)inputReq);
		CloseDevice((struct IORequest *)inputReq);
		DeleteIORequest((struct IORequest *)inputReq);
		inputReq = NULL;
	}

	if (inputPort) {
		DeleteMsgPort(inputPort);
		inputPort = NULL;
	}
}

static int AddInputHandler(void)
{
	if ((inputPort = CreateMsgPort())) {
		if ((inputReq = (struct IOStdReq *)CreateIORequest(inputPort, sizeof(*inputReq)))) {
			if (!OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)inputReq, 0)) {
				inputReq->io_Data = &inputHandler;
				inputReq->io_Command = IND_ADDHANDLER;
				if (!DoIO((struct IORequest *)inputReq))
				{
					return 0;
				}
			}
		}
	}

	RemoveInputHandler();

	return 1;
}


//
// initinput() -- init input system
//
int initinput(void)
{
	pointermem = (UWORD *)AllocVec(2 * 6, MEMF_CHIP | MEMF_CLEAR);
	inputdevices = 1|2; // keyboard (1) and mouse (2)
	mouseacquired = 0;
	if (LowLevelBase) {
		inputdevices |= 4; // joystick
		joynumbuttons = 15;
		joynumaxes = 0;
	}
	buildputs("Initialising game controllers\n");
	if ((gameport_mp = CreateMsgPort())) {
		if ((gameport_io = (struct IOStdReq *)CreateIORequest(gameport_mp, sizeof(struct IOStdReq)))) {
			int ix;
			BYTE gameport_ct;
			for (ix=0; ix<4; ix++) {
				if (!OpenDevice((STRPTR)"psxport.device", ix, (struct IORequest *)gameport_io, 0)) {
					buildprintf("psxport.device unit %d opened.\n", ix);
					Forbid();
					gameport_io->io_Command = GPD_ASKCTYPE;
					gameport_io->io_Length = 1;
					gameport_io->io_Data = &gameport_ct;
					DoIO((struct IORequest *)gameport_io);
					if (gameport_ct == GPCT_NOCONTROLLER) {
						gameport_ct = GPCT_ALLOCATED;
						gameport_io->io_Command = GPD_SETCTYPE;
						gameport_io->io_Length = 1;
						gameport_io->io_Data = &gameport_ct;
						DoIO((struct IORequest *)gameport_io);

						Permit();

						gameport_io->io_Command = GPD_SETTRIGGER;
						gameport_io->io_Length = sizeof(struct GamePortTrigger);
						gameport_io->io_Data = &gameport_gpt;
						DoIO((struct IORequest *)gameport_io);

						gameport_io->io_Command = GPD_READEVENT;
						gameport_io->io_Length = sizeof(struct InputEvent);
						gameport_io->io_Data = &gameport_ie;
						SendIO((struct IORequest *)gameport_io);
						gameport_is_open = TRUE;

						inputdevices |= 4;
						joynumbuttons = 15;
						joynumaxes = 6;

						break;
					} else {
						Permit();
						buildprintf("psxport.device unit %d in use.\n", ix);
						CloseDevice((struct IORequest *)gameport_io);
					}
				} else {
					buildprintf("psxport.device unit %d won't open.\n", ix);
				}
			}
		}
	}
	if (LowLevelBase && !gameport_is_open) {
		buildputs("Using lowlevel.library for joystick input\n");
	}

	return 0;
}

//
// uninitinput() -- uninit input system
//
void uninitinput(void)
{
	uninitmouse();

	if (gameport_is_open) {
		AbortIO((struct IORequest *)gameport_io);
		WaitIO((struct IORequest *)gameport_io);
		BYTE gameport_ct = GPCT_NOCONTROLLER;
		gameport_io->io_Command = GPD_SETCTYPE;
		gameport_io->io_Length = 1;
		gameport_io->io_Data = &gameport_ct;
		DoIO((struct IORequest *)gameport_io);
		CloseDevice((struct IORequest *)gameport_io);
		gameport_is_open = FALSE;
	}
	if (gameport_io != NULL) {
		DeleteIORequest((struct IORequest *)gameport_io);
		gameport_io = NULL;
	}
	if (gameport_mp != NULL) {
		DeleteMsgPort(gameport_mp);
		gameport_mp = NULL;
	}
	if (pointermem) {
		FreeVec(pointermem);
		pointermem = NULL;
	}
}

const char *getkeyname(int num)
{
	int i, scan;
	// linear search is not the fastest, but this is only used for the key binding menu
	for (i=0; i < (int)MAX_KEYCONV; i++) {
		scan = keyconv[i];
		if (scan != 0 && scan == num) {
			return keynames[i];
		}
	}
	return NULL;
}

static const char *llbuttonnames[15] = {
	"Dbl Red",
	"Dbl Blue",
	"Dbl Green",
	"Dbl Yellow",
	"N/A\0N/A",
	"N/A\0N/A",
	"Dbl Play",
	"N/A\0N/A",
	"N/A\0N/A",
	"Dbl Reverse",
	"Dbl Forward",
	"Dbl DPad Up",
	"Dbl DPad Down",
	"Dbl DPad Left",
	"Dbl DPad Right"
};

static const char *psxbuttonnames[15] = {
	"Dbl Cross",
	"Dbl Circle",
	"Dbl Square",
	"Dbl Triangle",
	"Dbl Select",
	"N/A\0N/A",
	"Dbl Start",
	"Dbl L3",
	"Dbl R3",
	"Dbl L1",
	"Dbl R1",
	"Dbl DPad Up",
	"Dbl DPad Down",
	"Dbl DPad Left",
	"Dbl DPad Right"
};

static const char *psxaxisnames[6] = {
	"Left Stick X",
	"Left Stick Y",
	"Right Stick X",
	"Right Stick Y",
	"L2",
	"R2",
};

const char *getjoyname(int what, int num)
{
	int button, notclicked;
	switch (what) {
		case 0: // axis
			return psxaxisnames[num];
		case 1: // button
			notclicked = !(num & 128);
			button = num & 127;
			if (button > 15) return NULL;
			return (gameport_is_open ? psxbuttonnames[button] : llbuttonnames[button]) + notclicked * 4;
		default:
			return NULL;
	}
}

//
// bgetchar, bkbhit, bflushchars -- character-based input functions
//
unsigned char bgetchar(void)
{
	unsigned char c;
	if (keyasciififoplc == keyasciififoend) return 0;
	c = keyasciififo[keyasciififoplc];
	keyasciififoplc = ((keyasciififoplc+1)&(KEYFIFOSIZ-1));
	return c;
}

int bkbhit(void)
{
	return (keyasciififoplc != keyasciififoend);
}

void bflushchars(void)
{
	keyasciififoplc = keyasciififoend = 0;
}


//
// set{key|mouse|joy}presscallback() -- sets a callback which gets notified when keys are pressed
//
void setkeypresscallback(void (*callback)(int, int)) { keypresscallback = callback; }
void setmousepresscallback(void (*callback)(int, int)) { mousepresscallback = callback; }
void setjoypresscallback(void (*callback)(int, int)) { joypresscallback = callback; }

//
// initmouse() -- init mouse input
//
int initmouse(void)
{
	moustat=1;
	grabmouse(1);
	return 0;
}

//
// uninitmouse() -- uninit mouse input
//
void uninitmouse(void)
{
	grabmouse(0);
	moustat=0;
}

static void constrainmouse(int a)
{
	if (!window) return;
	if (a) {
		if (pointermem && window->Pointer != pointermem) {
			SetPointer(window, pointermem, 1, 1, 0, 0);
		}
	} else {
		if (window->Pointer == pointermem) {
			ClearPointer(window);
		}
	}
}

//
// grabmouse() -- show/hide mouse cursor
//
void grabmouse(int a)
{
	if (appactive && moustat) {
		if (a != mouseacquired) {
			constrainmouse(a); // this can be called from the input handler
		}
	}
	mouseacquired = a;

	mousex = mousey = 0;
}


//
// readmousexy() -- return mouse motion information
//
void readmousexy(int *x, int *y)
{
	if (!mouseacquired || !appactive || !moustat) { *x = *y = 0; return; }
	*x = mousex;
	*y = mousey;
	mousex = mousey = 0;
}

//
// readmousebstatus() -- return mouse button information
//
void readmousebstatus(int *b)
{
	if (!mouseacquired || !appactive || !moustat) *b = 0;
	else *b = mouseb;
	// clear mousewheel events - the game has them now (in *b)
	// the other mousebuttons are cleared when there's a "button released"
	// event, but for the mousewheel that doesn't work, as it's released immediately
	mouseb &= ~(1<<4 | 1<<5);
}

//
// releaseallbuttons()
//
void releaseallbuttons(void)
{
	joyb = 0;
}


//
//
// ---------------------------------------
//
// All things Timer
// Ken did this
//
// ---------------------------------------
//
//

static unsigned int timerfreq=0;
static unsigned int timerlastsample=0;
static unsigned int timerticspersec=0;
static void (*usertimercallback)(void) = NULL;
#ifdef TIMERINT
static APTR g_timerIntHandle = NULL;
static struct EClockVal timeval;
static ULONG timecount = 0;
#else
static struct timerequest *timerio;
static struct MsgPort *timerport;
struct Device *TimerBase;
#endif

#ifdef TIMERINT
static SAVEDS ASM void BEL_ST_TimerInterrupt(REG(a1, APTR intData))
{
	totalclock++;
	if (usertimercallback)
		usertimercallback();
}
#endif

//
// inittimer() -- initialise timer
//
int inittimer(int tickspersecond)
{
	if (timerfreq) return 0;    // already installed

	buildputs("Initialising timer\n");

#ifdef TIMERINT
	if ((g_timerIntHandle = AddTimerInt((APTR)BEL_ST_TimerInterrupt, NULL)))
	{
		ULONG timerDelay = (1000 * 1000) / tickspersecond;
		timerfreq = timerticspersec = tickspersecond;
		StartTimerInt(g_timerIntHandle, timerDelay, TRUE);
		ElapsedTime(&timeval);
		timecount = 0;
	}
#else
	if ((timerport = CreateMsgPort()))
	{
		if ((timerio = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest))))
		{
			if (!OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timerio, 0))
			{
				TimerBase = timerio->tr_node.io_Device;
				timerfreq = 1000000; // usec
				timerticspersec = tickspersecond;
				timerlastsample = (getusecticks() * timerticspersec / timerfreq);
			}
			else
			{
				DeleteIORequest((struct IORequest *)timerio);
				DeleteMsgPort(timerport);
			}
		}
		else
		{
			DeleteMsgPort(timerport);
		}
	}
#endif

	usertimercallback = NULL;

	return 0;
}

//
// uninittimer() -- shut down timer
//
void uninittimer(void)
{
	if (!timerfreq) return;

#ifdef TIMERINT
	if (g_timerIntHandle)
	{
		StopTimerInt(g_timerIntHandle);
		RemTimerInt(g_timerIntHandle);
		g_timerIntHandle = NULL;
	}
#else
	if (!CheckIO((struct IORequest *)timerio))
	{
		AbortIO((struct IORequest *)timerio);
		WaitIO((struct IORequest *)timerio);
	}
	CloseDevice((struct IORequest *)timerio);
	DeleteIORequest((struct IORequest *)timerio);
	DeleteMsgPort(timerport);
	TimerBase = NULL;
#endif

	timerfreq=0;
}

//
// sampletimer() -- update totalclock
//
void sampletimer(void)
{
#ifndef TIMERINT
	int n;

	if (!timerfreq) return;

	n = (int)((double)getusecticks() * (double)timerticspersec / (double)timerfreq) - timerlastsample;
	if (n>0) {
		totalclock += n;
		timerlastsample += n;
	}

	if (usertimercallback) for (; n>0; n--) usertimercallback();
#endif
}

//
// getticks() -- returns a millisecond ticks count
//
unsigned int getticks(void)
{
	// this it can be called before inittimer
	//return (unsigned int)getusecticks() / 1000;
#if __GNUC__ <= 3
	struct DateStamp date;
	DateStamp(&date);
	ULONG sec = (date.ds_Days * 60 * 60 * 24) + (date.ds_Minute * 60);
	ULONG msec = date.ds_Tick * (1000 / 50);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ULONG sec = tv.tv_secs;
	ULONG msec = tv.tv_micro / 1000;
#endif
	return sec * 1000 + msec;
}

//
// getusecticks() -- returns a microsecond ticks count
//
unsigned int getusecticks(void)
{
#ifdef TIMERINT
	ULONG frac = ElapsedTime(&timeval);
	timecount += frac;
	return (unsigned int)((double)timecount*1000000.0/65536.0); // fixed to usec
#else
	struct timeval tv;
	GetSysTime(&tv);
	return (unsigned int)(tv.tv_secs * 1000000 + tv.tv_micro);
#endif
}


//
// gettimerfreq() -- returns the number of ticks per second the timer is configured to generate
//
int gettimerfreq(void)
{
	return timerticspersec;
}


//
// installusertimercallback() -- set up a callback function to be called when the timer is fired
//
void (*installusertimercallback(void (*callback)(void)))(void)
{
	void (*oldtimercallback)(void);

	oldtimercallback = usertimercallback;
	usertimercallback = callback;

	return oldtimercallback;
}



//
//
// ---------------------------------------
//
// All things Video
//
// ---------------------------------------
//
//


//
// getvalidmodes() -- figure out what video modes are available
//
static int sortmodes(const struct validmode_t *a, const struct validmode_t *b)
{
	int x;

	if ((x = a->fs   - b->fs)   != 0) return x;
	if ((x = a->bpp  - b->bpp)  != 0) return x;
	if ((x = a->xdim - b->xdim) != 0) return x;
	if ((x = a->ydim - b->ydim) != 0) return x;

	return 0;
}
static char modeschecked=0;
void getvalidmodes(void)
{
	static int defaultres[][2] = {
		{1600,1200},{1400,1050},
		{1280,1024},{1280,960},{1152,864},{1024,768},{800,600},{640,480},
		{640,400},{512,384},{480,360},{400,300},{320,240},{320,200},{0,0}
	};
	ULONG modeID;
	struct DimensionInfo diminfo;
	struct DisplayInfo dispinfo;
	int i, j, maxx=0, maxy=0;

	if (modeschecked) return;

	validmodecnt=0;

	buildputs("Detecting video modes:\n");

#define ADDMODE(x,y,c,f) if (validmodecnt<MAXVALIDMODES) { \
	int mn; \
	for(mn=0;mn<validmodecnt;mn++) \
		if (validmode[mn].xdim==x && validmode[mn].ydim==y && \
			validmode[mn].bpp==c  && validmode[mn].fs==f) break; \
	if (mn==validmodecnt) { \
		validmode[validmodecnt].xdim=x; \
		validmode[validmodecnt].ydim=y; \
		validmode[validmodecnt].bpp=c; \
		validmode[validmodecnt].fs=f; \
		validmodecnt++; \
		buildprintf("  - %dx%d %d-bit %s\n", x, y, c, (f&1)?"fullscreen":"windowed"); \
	} \
}

#define CHECKL(w,h) if ((w < maxx) && (h < maxy))
#define CHECKLE(w,h) if ((w <= maxx) && (h <= maxy))

	maxx = 320;
	maxy = 200;

	// Fullscreen 8-bit modes
	modeID = INVALID_ID;
	while ((modeID = NextDisplayInfo(modeID)) != (ULONG)INVALID_ID)
	{
		// mode id filter
		if (fsModeID != (ULONG)INVALID_ID && modeID != fsModeID)
			continue;

		// monitor id filter
		if (fsMonitorID != (ULONG)INVALID_ID && (modeID & MONITOR_ID_MASK) != fsMonitorID)
			continue;

		// no modes that can't do at least 8-bit
		if (!GetDisplayInfoData(NULL, (UBYTE *)&diminfo, sizeof(diminfo), DTAG_DIMS, modeID) || diminfo.MaxDepth < 8)
			continue;

		// no interlace modes as they are slow
		/*
		if (!GetDisplayInfoData(NULL, (UBYTE *)&dispinfo, sizeof(dispinfo), DTAG_DISP, modeID) || (dispinfo.PropertyFlags & DIPF_IS_LACE))
			continue;
		*/

		int width = diminfo.Nominal.MaxX + 1;
		int height = diminfo.Nominal.MaxY + 1;

		// does it fit one of the standard modes?
		for (i=0; defaultres[i][0]; i++) {
			// the width should match, but the height can be larger
			if (width == defaultres[i][0] && height >= defaultres[i][1]) {
				ADDMODE(defaultres[i][0],defaultres[i][1],8,1)
			}
		}
	}

	if (CyberGfxBase && fsMonitorID == (ULONG)INVALID_ID && fsModeID == (ULONG)INVALID_ID) {
		int bpp = 8;
		if ((screen = LockPubScreen((STRPTR)wndPubScreen))) {
			bpp = GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH);
			maxx = screen->Width;
			maxy = screen->Height;
			UnlockPubScreen(NULL, screen);
			screen = NULL;
			//buildprintf("screen %s maxx %d maxy %d bpp %d\n", wndPubScreen, maxx, maxy, bpp);
		}
		// Windowed mode requires 15 bit or higher color depth public screen
		if (bpp >= 15) {
			// Windowed 8-bit modes
			for (i=0; defaultres[i][0]; i++) {
				CHECKL(defaultres[i][0],defaultres[i][1]) {
					ADDMODE(defaultres[i][0], defaultres[i][1], 8, 0)
				}
			}
		}
	}

#undef CHECK
#undef ADDMODE

	qsort((void*)validmode, validmodecnt, sizeof(struct validmode_t), (int(*)(const void*,const void*))sortmodes);

	modeschecked=1;
}


//
// checkvideomode() -- makes sure the video mode passed is legal
//
int checkvideomode(int *x, int *y, int c, int fs, int forced)
{
	int i, nearest=-1, dx, dy, odx=9999, ody=9999;

	getvalidmodes();

	if (c > 8) return -1;

	// fix up the passed resolution values to be multiples of 8
	// and at least 320x200 or at most MAXXDIMxMAXYDIM
	if (*x < 320) *x = 320;
	if (*y < 200) *y = 200;
	if (*x > MAXXDIM) *x = MAXXDIM;
	if (*y > MAXYDIM) *y = MAXYDIM;
	*x &= 0xfffffff8l;

	for (i=0; i<validmodecnt; i++) {
		if (validmode[i].bpp != c) continue;
		if (validmode[i].fs != fs) continue;
		dx = klabs(validmode[i].xdim - *x);
		dy = klabs(validmode[i].ydim - *y);
		if (!(dx | dy)) {   // perfect match
			nearest = i;
			break;
		}
		if ((dx <= odx) && (dy <= ody)) {
			nearest = i;
			odx = dx; ody = dy;
		}
	}

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*x = validmode[nearest].xdim;
	*y = validmode[nearest].ydim;

	return nearest;     // JBF 20031206: Returns the mode number
}


static void shutdownvideo(void)
{
	RemoveInputHandler();

	if (frame) {
		//free(frame);
		FreeVec(frame);
		frame = NULL;
	}
	if (temprp.BitMap) {
		FreeBitMap(temprp.BitMap);
		temprp.BitMap = NULL;
	}
	if (window) {
		CloseWindow(window);
		window = NULL;
	}
	if (sbuf[0]) {
		FreeScreenBuffer(screen, sbuf[0]);
		sbuf[0] = NULL;
	}
	if (sbuf[1]) {
		FreeScreenBuffer(screen, sbuf[1]);
		sbuf[1] = NULL;
	}
	if (dispport) {
		//while (GetMsg(dispport));
		DeleteMsgPort(dispport);
		dispport = NULL;
	}
	if (screen) {
		CloseScreen(screen);
		screen = NULL;
	}
	use_c2p = 0;
}

static const char *getmonitorname(ULONG modeID)
{
	ULONG monitorid = (modeID & MONITOR_ID_MASK);

	switch (monitorid) {
		case PAL_MONITOR_ID:
			return "PAL";
		case NTSC_MONITOR_ID:
			return "NTSC";
		case DBLPAL_MONITOR_ID:
			return "DBLPAL";
		case DBLNTSC_MONITOR_ID:
			return "DBLNTSC";
		case EURO36_MONITOR_ID:
			return "EURO36";
		case EURO72_MONITOR_ID:
			return "EURO72";
		case SUPER72_MONITOR_ID:
			return "SUPER72";
		case VGA_MONITOR_ID:
			return "MULTISCAN";
	}

	if (CyberGfxBase && IsCyberModeID(modeID))
		return "RTG";

	return "UNKNOWN";
}

//
// setvideomode() -- set Amiga video mode
//
int setvideomode(int x, int y, int c, int fs)
{
	ULONG flags, idcmp;

	if ((fs == fullscreen) && (x == xres) && (y == yres) && (c == bpp) &&
		!videomodereset) {
		OSD_ResizeDisplay(xres,yres);
		return 0;
	}

	if (checkvideomode(&x,&y,c,fs,0) < 0) return -1;	// Will return if GL mode not available.

	shutdownvideo();

	buildprintf("Setting video mode %dx%d (%d-bpp %s)\n", x,y,c,
		(fs & 1) ? "fullscreen" : "windowed");

	if (fs) {
		ULONG modeID = INVALID_ID;

		if (fsModeID != (ULONG)INVALID_ID) {
			modeID = fsModeID;
			buildprintf("Using forced mode id: %08x\n", (int)fsModeID);
		} else if (fsMonitorID != (ULONG)INVALID_ID) {
			buildprintf("Using forced monitor: %s\n", getmonitorname(fsMonitorID));
		} else if (CyberGfxBase /*&& fsMonitorID == (ULONG)INVALID_ID*/) {
			modeID = BestCModeIDTags(
				CYBRBIDTG_Depth, 8,
				CYBRBIDTG_NominalWidth, x,
				CYBRBIDTG_NominalHeight, y,
				TAG_DONE);
		}

		if (modeID == (ULONG)INVALID_ID) {
			modeID = BestModeID(
				BIDTAG_NominalWidth, x,
				BIDTAG_NominalHeight, y,
				BIDTAG_Depth, 8,
				//BIDTAG_DIPFMustNotHave, SPECIAL_FLAGS|DIPF_IS_LACE,
				(fsMonitorID == (ULONG)INVALID_ID) ? TAG_IGNORE : BIDTAG_MonitorID, fsMonitorID,
				TAG_DONE);
		}

		// taken from build.c
		int whitecol = 0, blackcol = 0;
		int i, j, k, dark, light;
		dark = INT_MAX;
		light = 0;
		for(i=0;i<256;i++)
		{
			j = ((int)palette[i*3])+((int)palette[i*3+1])+((int)palette[i*3+2]);
			if (j > light) { light = j; whitecol = i; }
			if (j < dark) { dark = j; blackcol = i; }
		}
		//buildprintf("dark %d light %d whitecol %d blackcol %d\n", dark, light, whitecol, blackcol);

		struct TagItem vctl[] =
		{
			//{VTAG_SPEVEN_BASE_SET, 10*16},
			{VTAG_SPEVEN_BASE_SET, 0},
			{VTAG_BORDERBLANK_SET, TRUE},
			{VC_IntermediateCLUpdate, FALSE},
			{VTAG_END_CM, 0}
		};

		screen = OpenScreenTags(0,
			modeID != (ULONG)INVALID_ID ? SA_DisplayID : TAG_IGNORE, modeID,
			SA_Width, x,
			SA_Height, y,
			SA_Depth, 8,
			SA_ShowTitle, FALSE,
			SA_Quiet, TRUE,
			SA_Draggable, FALSE,
			SA_Type, CUSTOMSCREEN,
			SA_VideoControl, (IPTR)vctl,
			SA_DetailPen, blackcol,
			SA_BlockPen, whitecol,
			//SA_Interleaved, TRUE,
			//SA_Exclusive, TRUE,
			//SA_Overscan, OSCAN_STANDARD,
			TAG_DONE);
		
		/*VideoControl(screen->ViewPort.ColorMap, vctl);
		MakeScreen(screen);
		RethinkDisplay();*/

		SetRast(&screen->RastPort, blackcol);

		modeID = GetVPModeID(&screen->ViewPort);
		struct NameInfo nameinfo;
		if (GetDisplayInfoData(NULL, (UBYTE *)&nameinfo, sizeof(nameinfo), DTAG_NAME, modeID))
			buildprintf("Opened screen: 0x%08x %s\n", (int)modeID, nameinfo.Name);
		else
			buildprintf("Opened screen: 0x%08x %s\n", (int)modeID, getmonitorname(modeID));

		currentBitMap = 0;
		use_c2p = use_wcp = 0;

		if ((GetBitMapAttr(screen->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD)) {
			if ((x % 32) == 0 && (sbuf[0] = AllocScreenBuffer(screen, 0, SB_SCREEN_BITMAP)) && (sbuf[1] = AllocScreenBuffer(screen, 0, SB_COPY_BITMAP))) {
				safetochange = 1;
				dispport = CreateMsgPort();
				sbuf[0]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = dispport;
				sbuf[1]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = dispport;
				use_c2p = 1;
			} else if (((struct Library *)GfxBase)->lib_Version >= 40) {
				use_wcp = 1;
			} else {
				// allocate a temp rastport for WritePixelArray8
				struct BitMap *wpbm = AllocBitMap(screen->Width, 1, 8, BMF_CLEAR|BMF_STANDARD, NULL);
				if (wpbm) {
					CopyMem(window->RPort, &temprp, sizeof(struct RastPort));
					temprp.Layer = NULL;
					temprp.BitMap = wpbm;
				}
			}

		}
	}

	flags = WFLG_ACTIVATE | WFLG_RMBTRAP;
	idcmp = IDCMP_CLOSEWINDOW | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_RAWKEY;
	if (screen) {
		flags |= WFLG_BACKDROP | WFLG_BORDERLESS;
	} else {
		flags |=  WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET;
	}

	if (!screen && strcasecmp(wndPubScreen, "Workbench")) {
		buildprintf("Using forced public screen: %s\n", wndPubScreen);
	}

	window = OpenWindowTags(0,
		WA_InnerWidth, x,
		WA_InnerHeight, y,
		screen ? TAG_IGNORE : WA_Title, (IPTR)apptitle,
		WA_Flags, flags,
		screen ? WA_CustomScreen : TAG_IGNORE, (IPTR)screen,
		!screen ? WA_PubScreenName : TAG_IGNORE, (IPTR)wndPubScreen,
		WA_IDCMP, idcmp,
		TAG_DONE);

	if (window == NULL /*|| AddInputHandler()*/) {
		shutdownvideo();
		buildputs("Could not open the window");
		return -1;
	}

	if (!screen) {
		buildprintf("Opened window on public screen: %s\n", window->WScreen->Title);
	}
	
	AddInputHandler();

	if (c == 8) {
		int i, j, pitch;

		// Round up to a multiple of 4.
		//pitch = (((x|1) + 4) & ~3);
		// this would round 320 up to 324, just use the original
		pitch = x;

		//frame = (unsigned char *) malloc(pitch * y);
		frame = (unsigned char *) AllocVec(pitch * y, MEMF_FAST); // must be in fast mem
		if (!frame) {
			buildputs("Unable to allocate framebuffer\n");
			return -1;
		}

		frameplace = (intptr_t) frame;
		bytesperline = pitch;
		imageSize = bytesperline * y;
		numpages = 1;

		setvlinebpl(bytesperline);
		for (i = j = 0; i <= y; i++) {
			ylookup[i] = j;
			j += bytesperline;
		}

	} else {
		shutdownvideo();
		return -1;
	}

	xres = x;
	yres = y;
	bpp = c;
	fullscreen = fs;
	modechange = 1;
	videomodereset = 0;
	OSD_ResizeDisplay(xres,yres);

	// setpalettefade will set the palette according to whether gamma worked
	setpalettefade(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);

	if (mouseacquired) constrainmouse(1);

	startwin_close();

	return 0;
}


//
// resetvideomode() -- resets the video system
//
void resetvideomode(void)
{
	videomodereset = 1;
	modeschecked = 0;
}


//
// begindrawing() -- locks the framebuffer for drawing
//
void begindrawing(void)
{
}


//
// enddrawing() -- unlocks the framebuffer
//
void enddrawing(void)
{
}


//
// showframe() -- update the display
//
void showframe(void)
{
#ifdef __AROS__
	WaitTOF();
#endif
	if (screen) {
		if (use_c2p) {
			currentBitMap ^= 1;
			c2p_write_bm(bytesperline, yres, 0, 0, frame, sbuf[currentBitMap]->sb_BitMap);
			if (dispport) {
				if (!safetochange) {
					while (!GetMsg(dispport)) WaitPort(dispport);
				}
			} else {
				WaitTOF();
			}
			ChangeScreenBuffer(screen, sbuf[currentBitMap]);
			safetochange = 0;
		} else if (CyberGfxBase) {
			WritePixelArray(frame, 0, 0, bytesperline, window->RPort, 0, 0, xres, yres, RECTFMT_LUT8);
		} else if (use_wcp) {
			WriteChunkyPixels(window->RPort, 0, 0, xres - 1, yres - 1, frame, bytesperline);
		} else {
			WritePixelArray8(window->RPort, 0, 0, xres - 1, yres - 1, frame, &temprp);
		}
		if (updatePalette) {
			LoadRGB32(&screen->ViewPort, spal);
			updatePalette = 0;
		}
	} else {
		WriteLUTPixelArray(frame, 0, 0, bytesperline, window->RPort, ppal, window->BorderLeft, window->BorderTop, xres, yres, CTABFMT_XRGB8);
	}
}


//
// setpalette() -- set palette values
//
int setpalette(int UNUSED(start), int UNUSED(num), unsigned char * UNUSED(dapal))
{
	int i;

	if (screen) {
		//static ULONG spal[1 + (256 * 3) + 1];
		ULONG *sp = spal;

		*sp++ = 256 << 16;
		for (i = 0; i < 256; i++) {
			*sp++ = ((ULONG)curpalettefaded[i].r) << 24;
			*sp++ = ((ULONG)curpalettefaded[i].g) << 24;
			*sp++ = ((ULONG)curpalettefaded[i].b) << 24;
		}
		*sp = 0;

		//LoadRGB32(&screen->ViewPort, spal);
		updatePalette = 1;
	} else {
		unsigned char *pp = ppal;

		for (i = 0; i < 256; i++)
		{
#if B_BIG_ENDIAN != 0
			*pp++ = 0;
			*pp++ = curpalettefaded[i].r;
			*pp++ = curpalettefaded[i].g;
			*pp++ = curpalettefaded[i].b;
#else
			*pp++ = curpalettefaded[i].b;
			*pp++ = curpalettefaded[i].g;
			*pp++ = curpalettefaded[i].r;
			*pp++ = 0;
#endif
		}
	}

	return 0;
}

//
// setgamma
//
int setgamma(float gamma)
{
	return -1;
}


//
//
// ---------------------------------------
//
// Miscellany
//
// ---------------------------------------
//
//


static void updatejoystick(void)
{
	// We use SDL's game controller button order for BUILD:
	//   A, B, X, Y, Back, (Guide), Start, LThumb, RThumb,
	//   LShoulder, RShoulder, DPUp, DPDown, DPLeft, DPRight
	// So we must shuffle the buttons around.
	if (gameport_is_open) {
		// PSX joypad
		if (GetMsg(gameport_mp) != NULL) {
			if ((PSX_CLASS(gameport_ie) == PSX_CLASS_JOYPAD) || (PSX_CLASS(gameport_ie) == PSX_CLASS_WHEEL))
				analog_centered = FALSE;

			if (PSX_CLASS(gameport_ie) != PSX_CLASS_MOUSE) {
				ULONG gameport_curr = ~PSX_BUTTONS(gameport_ie);
				// buttons
				joyb = ((gameport_curr & PSX_CROSS) >> 6) | // A
					   ((gameport_curr & PSX_CIRCLE) >> 4) | // B
					   ((gameport_curr & PSX_SQUARE) >> 5) | // X
					   ((gameport_curr & PSX_TRIANGLE) >> 1) | // Y
					   ((gameport_curr & PSX_SELECT) >> 4) | // Back
					   ((gameport_curr & PSX_START) >> 5) | // Start
					   ((gameport_curr & PSX_L3) >> 2) | // LThumb
					   ((gameport_curr & PSX_R3) >> 2) | // RThumb
					   ((gameport_curr & PSX_L1) << 7) | // LShoulder
					   ((gameport_curr & PSX_R1) << 7) | // RShoulder
					   ((gameport_curr & PSX_UP) >> 1) | // DPUp
					   ((gameport_curr & PSX_DOWN) >> 2) | // DPDown
					   ((gameport_curr & PSX_LEFT) >> 2) | // DPLeft
					   ((gameport_curr & PSX_RIGHT) << 1); // DPRight
				// secondary trigger buttons as axes
				joyaxis[4] = (gameport_curr & PSX_L2) ? 32767 : 0;
				joyaxis[5] = (gameport_curr & PSX_R2) ? 32767 : 0;
			}

			if ((PSX_CLASS(gameport_ie) == PSX_CLASS_ANALOG) || (PSX_CLASS(gameport_ie) == PSX_CLASS_ANALOG2) || (PSX_CLASS(gameport_ie) == PSX_CLASS_ANALOG_MODE2)) {
				int analog_lx = PSX_LEFTX(gameport_ie);
				int analog_ly = PSX_LEFTY(gameport_ie);
				int analog_rx = PSX_RIGHTX(gameport_ie);
				int analog_ry = PSX_RIGHTY(gameport_ie);

				if (!analog_centered) {
					analog_clx = analog_lx;
					analog_cly = analog_ly;
					analog_crx = analog_rx;
					analog_cry = analog_ry;
					analog_centered = TRUE;
				}

				// left analog stick258
				joyaxis[0] = (analog_lx - analog_clx) << 8;
				joyaxis[1] = (analog_ly - analog_cly) << 8;
				// right analog stick
				joyaxis[2] = (analog_rx - analog_crx) << 8;
				joyaxis[3] = (analog_ry - analog_cry) << 8;
			}

			gameport_io->io_Command = GPD_READEVENT;
			gameport_io->io_Length = sizeof(struct InputEvent);
			gameport_io->io_Data = &gameport_ie;
			SendIO((struct IORequest *)gameport_io);
		}
	} else if (LowLevelBase) {
		// CD32 gamepad
		ULONG portState;
		portState = ReadJoyPort(1);
		joyb = ((portState & JPF_BUTTON_RED) >> JPB_BUTTON_RED) | // Red - A
			   (((portState & JPF_BUTTON_BLUE) >> JPB_BUTTON_BLUE) << 1) | // Blue - B
			   (((portState & JPF_BUTTON_GREEN) >> JPB_BUTTON_GREEN) << 2) | // Green - X
			   (((portState & JPF_BUTTON_YELLOW) >> JPB_BUTTON_YELLOW) << 3) | // Yellow - Y
			   (((portState & JPF_BUTTON_PLAY) >> JPB_BUTTON_PLAY) << 6) | // Play -> Start
			   (((portState & (JPF_BUTTON_FORWARD|JPF_BUTTON_REVERSE)) >> JPB_BUTTON_REVERSE) << 9) | // Reverse,Forward -> LShoulder,RShoulder
			   (((portState & JPF_JOY_UP) >> JPB_JOY_UP) << 11) | // Up
			   (((portState & JPF_JOY_DOWN) >> JPB_JOY_DOWN) << 12) | // Down
			   (((portState & JPF_JOY_LEFT) >> JPB_JOY_LEFT) << 13) | // Left
			   (((portState & JPF_JOY_RIGHT) >> JPB_JOY_RIGHT) << 14); // Right
	}
}

//
// handleevents() -- process the Amiga IDCMP message queue
//   returns !0 if there was an important event worth checking (like quitting)
//
static int capswaspressed = 0;
int handleevents(void)
{
	int rv=0;
	struct IntuiMessage *imsg;

	//if (!window) return rv;
	if (!window) {
		// probably in multiplayer mode, check for CTRL-C
		void __chkabort(void);
		__chkabort();
		return rv;
	}
	/*
	if (!window) {
		if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
			printf("%s CTRL-C\n", __FUNCTION__);
			quitevent = 1;
			return -1;
		} else {
			return 0;
		}
	}
	*/

	if (capswaspressed > 0) {
		capswaspressed--;
		if (capswaspressed <= 0) {
			capswaspressed = 0;
			SetKey(0x3a, 0);
			if (keypresscallback)
				keypresscallback(0x3a, 0);
			//buildprintf("%s caps lock release event\n", __FUNCTION__);
		}
	}

	while ((imsg = (struct IntuiMessage *)GetMsg(window->UserPort)))
	{
		switch (imsg->Class)
		{
		case IDCMP_RAWKEY:
			{
				int press = (imsg->Code & IECODE_UP_PREFIX) == 0;
				int code = imsg->Code & ~IECODE_UP_PREFIX;
				ULONG qualifier;
				unsigned char scan;
				int eatosdinput;

				if (code > (int)MAX_KEYCONV)
					break;

				qualifier = imsg->Qualifier;
				scan = keyconv[code];
				eatosdinput = 0;

				// the Caps Lock key only generates one of the key up/down events when pressed
				if (scan == 0x3a) {
					press = 1;
					capswaspressed = 5;
				}

				//OSD_Printf("rawkey %02x qual %04x code %02x press %d scan %x\n", imsg->Code, imsg->Qualifier, code, press, scan);
				// hook in the osd
				if (scan == 0) {
					// Not a key we want, so give it to the OS to handle.
					//continue;
				} else if (scan == OSD_CaptureKey(-1)) {
					if (press) {
						// The character produced by the OSD toggle key needs to be ignored.
						eatosdinput = 1;
						if ((qualifier & IEQUALIFIER_REPEAT) == 0) OSD_ShowDisplay(-1);
					}
					//continue;
				} else if (OSD_HandleKey(scan, press) != 0 && (qualifier & IEQUALIFIER_REPEAT) == 0) {
					SetKey(scan, press);
					if (keypresscallback)
						keypresscallback(scan, press);
				}

				if (eatosdinput) {
					eatosdinput = 0;
				} else if (press) {
#ifndef __AROS__
					// quick and dirty raw key to vanilla key conversion
					int ch = 0;
					if ((qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT /*| IEQUALIFIER_CAPSLOCK*/)) != 0) {
						ch = keyascii_shift[code];
					} else {
						ch = keyascii[code];
					}
					if ((qualifier & IEQUALIFIER_CAPSLOCK) != 0 && isalpha(ch))
					{
						if (isupper(ch))
							ch = tolower(ch);
						else
							ch = toupper(ch);
					}
					if (ch && OSD_HandleChar(ch)) {
						if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
							keyasciififo[keyasciififoend] = ch;
							keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
						}
					}
#else
					// map the key to ascii
					struct InputEvent ie;
					ie.ie_Class = IECLASS_RAWKEY;
					ie.ie_SubClass = 0;
					ie.ie_Code = imsg->Code;
					ie.ie_Qualifier = imsg->Qualifier;
					ie.ie_EventAddress = *((APTR **)imsg->IAddress);
					unsigned char bufascii;
					if (KeymapBase && MapRawKey(&ie, (STRPTR)&bufascii, sizeof(bufascii), NULL) > 0) {
						if (OSD_HandleChar(bufascii)) {
							if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
								keyasciififo[keyasciififoend] = bufascii;
								keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
							}
						}
					}
#endif
				}
			}
			break;
		case IDCMP_ACTIVEWINDOW:
			appactive = 1;
			rv=-1;
			break;
		case IDCMP_INACTIVEWINDOW:
			appactive = 0;
			rv=-1;
			break;
		case IDCMP_CLOSEWINDOW:
			quitevent = 1;
			rv=-1;
			break;
		}

		ReplyMsg((struct Message *)imsg);
	}

	updatejoystick();

	sampletimer();
	startwin_idle(NULL);

	//firstcall = 0;

	return rv;
}

#ifdef __libnix__
#if __GNUC__ <= 3
// don't touch ENV:
char *getenv(const char *value) { return NULL; }
// NOTES-3.4.0
int (putchar)(int c) { return fputc(c,stdout); }
#endif
// disable the default CTRL-C handler
//void __chkabort(void) {}
// replacement for the bugged ldiv
ldiv_t ldiv(long num, long denom)
{
	ldiv_t r;
	r.quot = num / denom;
	r.rem = num % denom;
	if (num >= 0 && r.rem < 0)
	{
		r.quot++;
		r.rem -= denom;
	}
	return r;
}
#endif
