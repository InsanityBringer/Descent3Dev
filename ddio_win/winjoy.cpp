/* 
* Descent 3 
* Copyright (C) 2024 Parallax Software
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "joystick.h"
#include "forcefeedback.h"
#include "pserror.h"									 
#include "pstypes.h"
#include "mem.h"
#include "ddio_win.h"
#include "Macros.h"
#include "logfile.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <regstr.h>
#include <stdlib.h>
#include <stdio.h>
#include <Xinput.h>


//////////////////////////////////////////////////////////////////////////////
struct tJoystickRecord
{
	ubyte valid;								// is this a valid device.
	ubyte flags;								// defined in ddio_win.h
	short spad;

	union 
	{
		int joyid;
		tDevice ffdev;
	}
	joy_context;								// used to identify the joystick.

	tJoyInfo caps;								// controller capabilities
	tJoyPos pos;								// current position
};


//////////////////////////////////////////////////////////////////////////////
static struct tJoystickLibraryData
{
	short init;									// library initialized?
	short njoy;									// maximum number of joysticks supported.
	tJoystickRecord *joystick;				// list of joysticks.

	LPDIRECTINPUT lpdi;						// if lpdi != NULL, we use direct input for joysticks
}
WJD = {0,0,NULL,NULL };



//////////////////////////////////////////////////////////////////////////////

bool joyw_get_name(int id, char *buf, const char *regkey);

bool joymm_init();
void joymm_close();
tJoystick joymm_init_stick(int joyid);
void joymm_close_stick(tJoystick handle);
void joymm_get_pos(tJoystick handle, tJoyPos *pos);

tJoystick joydi_init_stick(int ffjoyid);
void joydi_get_pos(tJoystick handle, tJoyPos *pos);

void joyxi_init(int id);

//	joystick forcefeedback shares
extern LPDIRECTINPUTDEVICE2 ddio_ff_get_joystick_obj(tDevice dev);

bool joy_chpro_hack = false;				// flightstick pro hack for Win32 only


//////////////////////////////////////////////////////////////////////////////
// initializes joystick interface.

bool joy_Init(bool emulation)
{
	//Can we initialize DirectInput version?
	//Try to init forcefeedback (which initializes joysticks)
	uint i, n; // = joyGetNumDevs();

	ddio_ff_AttachForce();

// initializes forcefeedback system?
	if(ddio_ff_Init()){
		mprintf((0,"DDForce: Force Feedback Joystick Found\n"));
	}	

// initialize data structures.
	n = 15;
	if (n) {
		WJD.joystick = (tJoystickRecord *)mem_malloc(sizeof(tJoystickRecord)*n);
		for (i = 0;i < n;i++)
		{
			WJD.joystick[i].valid = 0;
		}
	}
	else {
		WJD.init = 0;
		return false;
	}
	WJD.njoy = n;

// attempt directinput initialization.
	int dio_count = (!emulation) ? ddio_ffjoy_Init() : 0;
	XINPUT_STATE _discard;
	int xinput_success = XInputGetState(0, &_discard) == ERROR_SUCCESS
	|| XInputGetState(1, &_discard) == ERROR_SUCCESS
	|| XInputGetState(2, &_discard) == ERROR_SUCCESS
	|| XInputGetState(3, &_discard) == ERROR_SUCCESS;

    if (xinput_success) {
		for (i = 0; i < 4; i++) {
			joyxi_init(i);
		}
	}
	else if(dio_count > 0) {
		WJD.lpdi = DInputData.lpdi;

	// map all ffjoysticks to our joystick structure.
		for (i = 0; i < dio_count; i++)
		{
			joydi_init_stick(i);
		}

	}
//	enumerate all windows joysticks (1 and 2 only)
	else if (joymm_init()) {
		WJD.lpdi = NULL;
	}
	else {
		WJD.init = 0;
		return false;
	}

	WJD.init = 1;

	return true;
}


void joy_Close()
{
	ddio_ff_DetachForce();

	if (!WJD.init) return;

	if (!WJD.lpdi) {
		joymm_close();
	}

	mem_free(WJD.joystick);
	WJD.joystick = NULL;
	WJD.njoy = 0;
	WJD.lpdi = NULL;
	WJD.init = 0;
}


//	returns true if joystick valid
bool joy_IsValid(tJoystick handle)
{
	if (!WJD.init) return false;
	if (handle < 0 || handle >= WJD.njoy) return false;

	return WJD.joystick[handle].valid ? true : false;
}


//	retreive uncalibrated position of joystick
void joy_GetRawPos(tJoystick joy, tJoyPos *pos)
{
	joy_GetPos(joy, pos);
	pos->x = (pos->x+128)*JOYAXIS_RANGE;
	pos->y = (pos->y+128)*JOYAXIS_RANGE;
	pos->z = (pos->z+128)*JOYAXIS_RANGE;
	pos->r = (pos->r+128)*JOYAXIS_RANGE;
	pos->u = (pos->u+128)*JOYAXIS_RANGE;
	pos->v = (pos->v+128)*JOYAXIS_RANGE;
}



//	returns the state of a stick, remote or otherwise
void joy_GetPos(tJoystick stick, tJoyPos *pos)
{
	ASSERT(stick >= 0 || stick < WJD.njoy);
	int i;

	for (i = 0; i < JOYPOV_NUM; i++)
	{
		pos->pov[i] = JOYPOV_CENTER;
	}

	if (stick < 0 || stick >= WJD.njoy) {
		memset(pos, 0, sizeof(tJoyPos));
		return;
	}
	if (!WJD.joystick[stick].valid) {
		memset(pos, 0, sizeof(tJoyPos));
		return;
	}

	

	if (WJD.lpdi) {
		joydi_get_pos(stick, pos);
	}
	else {
		XINPUT_STATE state = {0};
		if (XInputGetState(stick, &state) == ERROR_SUCCESS) {
			pos->x = (int)(state.Gamepad.sThumbLX/32767.0f*128.0f);
			pos->y = -(int)(state.Gamepad.sThumbLY/32767.0f*128.0f);
			pos->z = (int)(state.Gamepad.bLeftTrigger/255.0f*128.0f);
			pos->r = (int)(state.Gamepad.sThumbRX/32767.0f*128.0f);
			pos->u = (int)(state.Gamepad.sThumbRY/32767.0f*128.0f);
			pos->v = (int)(state.Gamepad.bRightTrigger/255.0f*128.0f);
			pos->buttons = state.Gamepad.wButtons;
			pos->buttons |= (state.Gamepad.bLeftTrigger > 0) ? (1<<16) : 0;
			pos->buttons |= (state.Gamepad.bRightTrigger > 0) ? (1<<17) : 0;
			pos->btn = 0;
			return;
		}

		joymm_get_pos(stick, pos);
	}
}


//	retreive information about joystick.
void joy_GetJoyInfo(tJoystick joy, tJoyInfo *info)
{
	tJoyInfo *srcinfo;
	ASSERT(joy >= 0 || joy < WJD.njoy);

	if (!WJD.init) return;
	if (!WJD.joystick[joy].valid) return;
	srcinfo = &WJD.joystick[joy].caps;

	memcpy(info, srcinfo, sizeof(tJoyInfo));
}


// hook into ddio system if joystick needs any additonal processing per frame.
void ddio_InternalJoyFrame()
{
	if (!WJD.init) return;
	
	if (WJD.lpdi) {
		int i;
		tJoystickRecord *joystick = &WJD.joystick[0];
		for (i = 0; i < WJD.njoy; i++)
		{
			int try_count = 0;

			if (joystick->valid) {
				LPDIRECTINPUTDEVICE2 lpdidev = ddio_ff_get_joystick_obj(joystick->joy_context.ffdev);
			
			retry_joy_poll:
				HRESULT hr = lpdidev->Poll();
				if (hr == DIERR_INPUTLOST && try_count < 2) {
					ddio_ff_Acquire(joystick->joy_context.ffdev);
					try_count++;
					goto retry_joy_poll;
				}
			}
			joystick++;
		}
	}
}

void joyxi_init(int i)
{
	XINPUT_STATE state = {0};
	if (XInputGetState(i, &state) == ERROR_SUCCESS) {
		WJD.joystick[i].valid = 1;
		WJD.joystick[i].joy_context.joyid = i;
		WJD.joystick[i].caps.axes_mask = 
		JOYFLAG_XVALID+JOYFLAG_YVALID+
		JOYFLAG_RVALID+JOYFLAG_UVALID+JOYFLAG_VVALID+
		JOYFLAG_ZVALID;
		WJD.joystick[i].caps.num_btns = 18;
		strcpy(WJD.joystick[i].caps.name, "XInput Joystick");
		WJD.joystick[i].caps.minx = -32768;
		WJD.joystick[i].caps.miny = -32768;
		WJD.joystick[i].caps.minz = 0;
		WJD.joystick[i].caps.minr = -32768;
		WJD.joystick[i].caps.minu = -32768;
		WJD.joystick[i].caps.minv = -0;
		
		WJD.joystick[i].caps.maxx = 32767;
		WJD.joystick[i].caps.maxy = 32767;
		WJD.joystick[i].caps.maxz = 255;
		WJD.joystick[i].caps.maxr = 32767;
		WJD.joystick[i].caps.maxu = 32767;
		WJD.joystick[i].caps.maxv = 255;
	}
}

//////////////////////////////////////////////////////////////////////////////
// initializes multimedia version of joystick interface.
bool joymm_init()
{
	joymm_init_stick(JOYSTICKID1);
	if (!joy_chpro_hack) 
	{
		for (int i = 1; i < WJD.njoy; i++)
		{
			joymm_init_stick(JOYSTICKID1+i);
		}
	}

	return true;
}


// frees joystick library.
void joymm_close()
{
	int i;
	for (i = 0; i < WJD.njoy; i++)
	{
		if (WJD.joystick[i].valid) {
			joymm_close_stick(WJD.joystick[i].joy_context.joyid);
		}
	}
}


//	initializes one stick.
tJoystick joymm_init_stick(int joyid)
{
	int i;
	
// find free joystick slot
	for (i = 0; i < WJD.njoy; i++)
	{
		if (!WJD.joystick[i].valid) {
		// found slot, lets initialize it and return.
			JOYCAPS jc;

			if (joyGetDevCaps(joyid, &jc, sizeof(jc))==JOYERR_NOERROR) {
				tJoyInfo *caps = &WJD.joystick[i].caps;
				
				WJD.joystick[i].valid = 1;
				WJD.joystick[i].joy_context.joyid = joyid;
				
				memset(caps, 0, sizeof(tJoyInfo));
				caps->axes_mask = ((jc.wCaps & JOYCAPS_HASR) ? JOYFLAG_RVALID : 0) |
										((jc.wCaps & JOYCAPS_HASZ) ? JOYFLAG_ZVALID : 0) |
										((jc.wCaps & JOYCAPS_HASU) ? JOYFLAG_UVALID : 0) |
										((jc.wCaps & JOYCAPS_HASV) ? JOYFLAG_VVALID : 0) |
										((jc.wCaps & JOYCAPS_HASPOV) ? JOYFLAG_POVVALID : 0) | (JOYFLAG_XVALID+JOYFLAG_YVALID);
				caps->num_btns = (joy_chpro_hack) ? 32 : jc.wNumButtons;
				caps->minx = jc.wXmin;
				caps->miny = jc.wYmin;
				caps->minz = jc.wZmin;
				caps->minr = jc.wRmin;
				caps->minu = jc.wUmin;
				caps->minv = jc.wVmin;
				caps->maxx = jc.wXmax;
				caps->maxy = jc.wYmax;
				caps->maxz = jc.wZmax;
				caps->maxr = jc.wRmax;
				caps->maxu = jc.wUmax;
				caps->maxv = jc.wVmax;

				if (!joyw_get_name(joyid, caps->name, jc.szRegKey)) {
					sprintf(caps->name, "Joystick-%d", i);
				}

				return (tJoystick)(i);
			}
			else {
				return (tJoystick)(-1);
			}
		}
	}

	return -1;
}


void joymm_close_stick(tJoystick handle)
{
	if (handle < 0 || handle >= WJD.njoy) return;

	WJD.joystick[handle].valid = 0;
}


void joymm_get_pos(tJoystick handle, tJoyPos *pos)
{
	JOYINFOEX ji;
	tJoyInfo *caps;
	
	if (handle < 0 || handle >= WJD.njoy) {
		return;
	}
	if (!WJD.joystick[handle].valid) {
		return;
	}

	caps = &WJD.joystick[handle].caps;

	ZeroMemory(&ji, sizeof(JOYINFOEX));
	
	ji.dwSize = sizeof(JOYINFOEX);
	ji.dwFlags = JOY_RETURNCENTERED | JOY_USEDEADZONE | 
							JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV |
							JOY_RETURNPOV | JOY_RETURNBUTTONS;
	
	if (joyGetPosEx(WJD.joystick[handle].joy_context.joyid, &ji) != JOYERR_NOERROR) {
		memset(pos, 0, sizeof(tJoyPos));
		pos->pov[0] = JOYPOV_CENTER;
		return;	
	}
	
	pos->x = (int)((ji.dwXpos<<8)/(caps->maxx - caps->minx)) - 128;
	pos->y = (int)((ji.dwYpos<<8)/(caps->maxy - caps->miny)) - 128;

	if (caps->axes_mask & JOYFLAG_ZVALID) {
		pos->z = (int)((ji.dwZpos<<8)/(caps->maxz - caps->minz)) - 128;
	}
	else { pos->z = 0; }
	if (caps->axes_mask & JOYFLAG_RVALID) {
		pos->r = (int)((ji.dwRpos<<8)/(caps->maxr - caps->minr)) - 128;
	}
	else { pos->r = 0; }
	if (caps->axes_mask & JOYFLAG_UVALID) {
		pos->u = (int)((ji.dwUpos<<8)/(caps->maxu - caps->minu)) - 128;
	}
	else { pos->u = 0; }
	if (caps->axes_mask & JOYFLAG_VVALID) {
		pos->v = (int)((ji.dwVpos<<8)/(caps->maxv - caps->minv)) - 128;
	}
	else { pos->v = 0; }
		
	if (caps->axes_mask & JOYFLAG_POVVALID) {
		ji.dwPOV = ji.dwPOV & 0x0000ffff;
		if (ji.dwPOV == JOY_POVCENTERED) 
			pos->pov[0] = JOYPOV_CENTER;
		else 
			pos->pov[0] = (unsigned)(ji.dwPOV*JOYPOV_MAXVAL/35900.0f);
	}
	else {
		pos->pov[0] = JOYPOV_CENTER;
	}

	pos->buttons = 0;
	if (joy_chpro_hack) {
		if (ji.dwButtons > 0 && ji.dwButtons <=32) {
			pos->buttons = 1<<(ji.dwButtons-1);
			mprintf((0, "btns=%x\n", pos->buttons));
		}
	}
	else {
		pos->buttons = (unsigned)ji.dwButtons;
	}

	pos->btn = (unsigned)ji.dwButtonNumber;

#ifdef _DEBUG
	mprintf_at((4,handle+1,0, "%d:X:%04d Y:%04d Z:%04d %d:R:%04d U:%04d V:%04d", handle, pos->x,pos->y,pos->z,pos->r,pos->u,pos->v,pos->buttons));
#endif
}


void joydi_get_pos(tJoystick handle, tJoyPos *pos)
{
	tJoyInfo *caps;
	tJoystickRecord *joystick;
	LPDIRECTINPUTDEVICE2 lpdidev2;
	DIJOYSTATE ji;
	HRESULT hr;
	int try_count=0;
	unsigned i;
	
	if (handle < 0 || handle >= WJD.njoy) return;
	if (!WJD.joystick[handle].valid) return;

	joystick = &WJD.joystick[handle];
	caps = &joystick->caps;
	lpdidev2 = ddio_ff_get_joystick_obj(joystick->joy_context.ffdev);

	if (!lpdidev2) {	return; }
	
retry_joy_input:
	hr = lpdidev2->GetDeviceState(sizeof(ji), &ji);
	if (hr == DIERR_NOTACQUIRED && try_count < 4) {
		ddio_ff_Acquire(joystick->joy_context.ffdev);
		try_count++;
		goto retry_joy_input;
	}
	else if (hr != DI_OK) {
		memset(pos, 0, sizeof(pos));
		pos->pov[0] = JOYPOV_CENTER;
	}

// interpret data.
	if (caps->axes_mask & JOYFLAG_XVALID) {
		pos->x = (int)((ji.lX<<8)/(caps->maxx - caps->minx)) - 128;
	}
	else { pos->x = 0; }
	if (caps->axes_mask & JOYFLAG_YVALID) {
		pos->y = (int)((ji.lY<<8)/(caps->maxy - caps->miny)) - 128;
	}
	else { pos->y = 0; }
	if (caps->axes_mask & JOYFLAG_ZVALID) {
		pos->z = (int)((ji.lZ<<8)/(caps->maxz - caps->minz)) - 128;
	}
	else { pos->z = 0; }
	if (caps->axes_mask & JOYFLAG_RVALID) {
		pos->r = (int)((ji.lRz<<8)/(caps->maxr - caps->minr)) - 128;
	}
	else { pos->r = 0; }
	if (caps->axes_mask & JOYFLAG_UVALID) {
		pos->u = (int)((ji.rglSlider[0]<<8)/(caps->maxu - caps->minu)) - 128;
	}
	else { pos->u = 0; }
	if (caps->axes_mask & JOYFLAG_VVALID) {
		pos->v = (int)((ji.rglSlider[1]<<8)/(caps->maxv - caps->minv)) - 128;
	}
	else { pos->v = 0; }

	for (i = 0; i < 4; i++)
	{
		if (caps->axes_mask & (JOYFLAG_POVVALID << i)) {
			if (loword(ji.rgdwPOV[i]) == 0xffff) {
				pos->pov[i] = JOYPOV_CENTER;
			}
			else {
				int index = (ji.rgdwPOV[i]/4500);
				pos->pov[i] = index * 0x20;
		//@@	switch (index)
		//@@	{
		//@@	case 0: pos->pov = JOYPOV_UP; break;
		//@@	case 1: pos->pov = JOYPOV_RIGHT; break;
		//@@	case 2: pos->pov = JOYPOV_DOWN; break;
		//@@	case 3: pos->pov = JOYPOV_LEFT; break;
		//@@	default: pos->pov = JOYPOV_CENTER;
		//@@	}
			}
		}
		else {
			pos->pov[i] = JOYPOV_CENTER;
		}
	}

	pos->buttons = 0;
	for (i = 0; i < caps->num_btns; i++)
	{
		if (ji.rgbButtons[i] & 0x80) {
			pos->buttons |= (1<<i);
			pos->btn = i;
		}
	}

#ifdef _DEBUG
	mprintf_at((4,(handle*2)+1,0, "J%d:X:%04d Y:%04d Z:%04d Rz:%04d S0:%04d S1:%04d", handle, pos->x,pos->y,pos->z,pos->r,pos->u,pos->v));
	mprintf_at((4,(handle*2)+2,0, "   POV0:%04d POV1:%04d POV2:%04d POV3:%04d", handle, pos->pov[0], pos->pov[1], pos->pov[2], pos->pov[3]));
#endif
}


//	direct input implementation.
tJoystick joydi_init_stick(int ffjoyid)
{
	LPDIRECTINPUTDEVICE2 lpdidev2;

	lpdidev2 = ddio_ff_get_joystick_obj((tDevice)ffjoyid);
	if (lpdidev2) {
	// find free joystick slot
		int i;

		for (i = 0; i < WJD.njoy; i++)
		{
			if (!WJD.joystick[i].valid) {
			// found slot, lets initialize it and return.
			// get axis, and each of their ranges
				DIDEVICEOBJECTINSTANCE  DIObjInst; 
				DIDEVICEINSTANCE	DIDevInst;
				DIPROPRANGE dirange;
				DIDEVCAPS dicaps;
				tDevice dev = (tDevice)ffjoyid;
				tJoyInfo *caps = &WJD.joystick[i].caps;

				memset(caps, 0, sizeof(tJoyInfo));

				WJD.joystick[i].joy_context.ffdev = dev;
				WJD.joystick[i].valid = 1;

			// get device info
				DIDevInst.dwSize = sizeof(DIDEVICEINSTANCE);
				if (lpdidev2->GetDeviceInfo(&DIDevInst) == DI_OK) {
					strcpy(WJD.joystick[i].caps.name, DIDevInst.tszProductName);
				}
				else {
					sprintf(WJD.joystick[i].caps.name, "Joystick-%d", i);
				}

			// get device object info
				ddio_ff_Acquire(dev);
				dicaps.dwSize = sizeof(DIDEVCAPS);

				if (lpdidev2->GetCapabilities(&dicaps) != DI_OK) {
					mprintf((0,"ddio_ffjoy_Query: Failed getting device caps\n"));
					return -1;
				}

				DIObjInst.dwSize = sizeof(DIDEVICEOBJECTINSTANCE); 

				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_X, DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_XVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_Y, DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_YVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_Z, DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_ZVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_RZ, DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_RVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_SLIDER(0), DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_UVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_SLIDER(1), DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_VVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_POV(0), DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_POVVALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_POV(1), DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_POV2VALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_POV(2), DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_POV3VALID;
				}
				if (lpdidev2->GetObjectInfo(&DIObjInst, DIJOFS_POV(3), DIPH_BYOFFSET)==DI_OK) {
					caps->axes_mask |= JOYFLAG_POV4VALID;
				}

				caps->num_btns = dicaps.dwButtons;

				dirange.diph.dwSize = sizeof(DIPROPRANGE);
				dirange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
				dirange.diph.dwHow = DIPH_BYOFFSET;
				
				dirange.diph.dwObj = DIJOFS_X;
				if (lpdidev2->GetProperty(DIPROP_RANGE, (DIPROPHEADER *)&dirange)==DI_OK) {
					caps->minx = (int)dirange.lMin;
					caps->maxx = (int)dirange.lMax;
				}
				dirange.diph.dwObj = DIJOFS_Y;
				if (lpdidev2->GetProperty(DIPROP_RANGE, (DIPROPHEADER *)&dirange)==DI_OK) {
					caps->miny = (int)dirange.lMin;
					caps->maxy = (int)dirange.lMax;
				}
				dirange.diph.dwObj = DIJOFS_Z;
				if (lpdidev2->GetProperty(DIPROP_RANGE, (DIPROPHEADER *)&dirange)==DI_OK) {
					caps->minz = (int)dirange.lMin;
					caps->maxz = (int)dirange.lMax;
				}
				dirange.diph.dwObj = DIJOFS_RZ;
				if (lpdidev2->GetProperty(DIPROP_RANGE, (DIPROPHEADER *)&dirange)==DI_OK) {
					caps->minr = (int)dirange.lMin;
					caps->maxr = (int)dirange.lMax;
				}
				dirange.diph.dwObj = DIJOFS_SLIDER(0);
				if (lpdidev2->GetProperty(DIPROP_RANGE, (DIPROPHEADER *)&dirange)==DI_OK) {
					caps->minu = (int)dirange.lMin;
					caps->maxu = (int)dirange.lMax;
				}
				dirange.diph.dwObj = DIJOFS_SLIDER(1);
				if (lpdidev2->GetProperty(DIPROP_RANGE, (DIPROPHEADER *)&dirange)==DI_OK) {
					caps->minv = (int)dirange.lMin;
					caps->maxv = (int)dirange.lMax;
				}

				lpdidev2->Poll();

				return (tJoystick)(i);
			}
			else {
				return (tJoystick)(-1);
			}
		}
	}

	Int3();
	return -1;
}





// JAS: Taken from Nov 1996 Game Developer, page 49.
// Damn, that is some UGLY code!  (But it works...)
/*---------------------------------------------------------------------*/    
bool joyw_get_name(int joy_num, char *szReturnName, const char *szRegKey)
/* 
   Description :  	Opens the MediaResources\Joysitick\mjstick.drv<xxxx>\JoystickSettings and 
					extracts Joystick%dOEMName string
   Arguments :     	joy_num    (r/o) - Joystick Number
					szRegKey   (r/o) - Registry Key of the msjstick.drv
					ReturnName (r/w) - Return String for name listed in Control Panel                                               
   Returns :		0 for success 1 for failure
/*-----------------------------------------------------------------------*/ 
{
   BYTE KeyStr[MAX_PATH] = REGSTR_PATH_JOYCONFIG;       	// found in regstr.h
   BYTE KeyJoySetStr[MAX_PATH] = REGSTR_KEY_JOYSETTINGS; 	// found in Regstr.h
   BYTE szOEMName[MAX_PATH];                             	// OEM name from Current Settings
   HKEY ConfigKey;
   HKEY JoyConfigKey;                                    	// Joystick Configuration
   HKEY DriverKey;                                       	// Joystick Driver Key
   HKEY OEMPropKey;
   HKEY PropKey;
   DWORD Length;
   if( ERROR_SUCCESS != RegOpenKey( HKEY_LOCAL_MACHINE,REGSTR_PATH_JOYCONFIG,&ConfigKey ) )

      {
      return( false );                	// It should never get here key received from Caps
      }                              
  
   if( ERROR_SUCCESS != RegOpenKey( ConfigKey, szRegKey, &DriverKey ) )
      {
      return( false );       	// It should never get here key received from Caps
      }                              
                                                	// Open CurrentSettings Key
   
   if( ERROR_SUCCESS != RegOpenKey( DriverKey, REGSTR_KEY_JOYCURR, &JoyConfigKey ) )
      {
      return( false );                  	// It should never get here always a Current Settings
      }
   sprintf((char *)KeyStr,(char *)REGSTR_VAL_JOYNOEMNAME,joy_num+1);   
   Length=sizeof(szOEMName);                        	// Get OEMNAME Configuration
   
   if( ERROR_SUCCESS != RegQueryValueEx( JoyConfigKey,(char *)KeyStr,NULL,NULL,(unsigned char *)&szOEMName,&Length))
      {
        return( false );                                	// No OEM name listed return error
      }
   RegCloseKey( ConfigKey );      	// Closes the registry Key
                                                    	
	// Open OEM Properties Key
    if( ERROR_SUCCESS != RegOpenKey(HKEY_LOCAL_MACHINE,REGSTR_PATH_JOYOEM,&PropKey ) )
      {
      return( false );                  	// It should never get here key received from Caps
      }                              
   
   if( ERROR_SUCCESS != RegOpenKey( PropKey, (char *)szOEMName, &OEMPropKey ) )
      {
      return( false );                  	// It should never get here if device is selected
      }
   Length=MAX_PATH;                        	// Get Name as listed in Control Panel
   
   if( ERROR_SUCCESS != RegQueryValueEx( OEMPropKey,REGSTR_VAL_JOYOEMNAME,NULL,NULL,(unsigned char *)szReturnName,&Length))
      {
        return( false );                              	 // No OEM name listed return error
      }
   RegCloseKey( OEMPropKey );         	// Closes the registry Key
   return true;

} /* End GetJoystickName */
