// Module for DirectInput
#include <xtl.h>
#include "burner.h"
#include <math.h>
// Key codes
#include "inp_keys.h"

#define LIST_DEVICES

#define MAX_KEYBOARD	(0)
#define MAX_JOYSTICK	(4)
#define MAX_JOYAXIS		(4)
#define MAX_MOUSE		(0)
#define MAX_MOUSEAXIS	(0)

const int DEADZONE = 16000;

int	 nPlayerToGamepad[4] = {0, 1, 2, 3};
static int nJoystickCount = 4;		// We poll all 4 gamepad Xbox ports by default
extern int ArcadeJoystick;
extern bool DoReset;

// Set up a single joystick

static int XInput2JoystickInitSingle(int i)
{

	return 0;
}

// Get a DI interface to each joystick
static int XInput2JoystickInitMaster()
{

	return 0;
}

// ----------------------------------------------------------------------------

// Get a DI interface to all mice
static int XInput2MouseInitMaster()
{

	return 0;
}

// ----------------------------------------------------------------------------

int XInput2SetCooperativeLevel(bool bExclusive, bool bForeGround)
{


	return 0;
}

// ----------------------------------------------------------------------------

int XInput2Exit()
{


	return 0;
}

int XInput2Init()
{


	return 0;
}

// ----------------------------------------------------------------------------

// Call before checking for Input in a frame
int XInput2Start()
{	
	return 0;
}
 

// Get the state (pressed = 1, not pressed = 0) of a particular input code
 
int XInput2State(int nCode)
{
	DWORD dwResultP1;
	DWORD dwResultP2;
	DWORD dwResultP3;
	DWORD dwResultP4;

	XINPUT_STATE stateJoy1;
	XINPUT_STATE stateJoy2;	 	 	
	XINPUT_STATE stateJoy3;	 	 	
	XINPUT_STATE stateJoy4;	 	 	

	if (nCode < 0) {
		return 0;
	}

    // Simply get the state of the controller from XInput.

    dwResultP1 = XInputGetState( 0, &stateJoy1 );
	dwResultP2 = XInputGetState( 1, &stateJoy2 );
	dwResultP3 = XInputGetState( 2, &stateJoy3 );
	dwResultP4 = XInputGetState( 3, &stateJoy4 );

	if (ArcadeJoystick==1)
	{
		if (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_BACK  && 
			stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_START )
		{
			Sleep(100);
			AudBlankSound();
			SetPauseMode(1);			
			return 0;
		}
	}
	else
	{

		if( ( stateJoy1.Gamepad.bLeftTrigger > 128 ) &&
			( stateJoy1.Gamepad.bRightTrigger > 128 ) &&
			( stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) ) // Reboot the dev kit
		{
			AudBlankSound();		
			SetPauseMode(1);			
			return 0;

		}
	}
	

	if (DoReset)
	{
		
		if (nCode == FBK_F3)
		{
			DoReset = false;
			return 1;
		}
		
	}

	switch (nCode)
	{
		case 0x06:
			return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? 1 : 0);
		case 0x02:
			return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_START ? 1 : 0);
		case 0xC8: 
			return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP    ? 1 : 0 || stateJoy1.Gamepad.sThumbLY >  DEADZONE);		// Up
		case 0xD0: 
			return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? 1 : 0 || stateJoy1.Gamepad.sThumbLY < -DEADZONE);		// Down
		case 0xCB: 
			return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? 1 : 0 || stateJoy1.Gamepad.sThumbLX < -DEADZONE);		// Left
		case 0xCD: 
			return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 1 : 0 || stateJoy1.Gamepad.sThumbLX >  DEADZONE);		// Right

		case 0x2C: return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_A ? 1 : 0);				// fire 1			
		case 0x2D: return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_B ? 1 : 0);				// fire 2			
		case 0x2E: return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_X ? 1 : 0);				// fire 3			
		case 0x2F: return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_Y ? 1 : 0);				// fire 4		
		case 0x1F: return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 1 : 0);			// fire 5			
		case 0x20: return (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 1 : 0);			// fire 6		

		case 0x88:
			return ( stateJoy1.Gamepad.bLeftTrigger > 128 ? 1 : 0);
		case 0x8A:			 
			return ( stateJoy1.Gamepad.bRightTrigger > 128 ? 1 : 0);

		case 0x3b:
			return ( stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB  ? 1 : 0);
		case 0x3c:
			return ( stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB  ? 1 : 0);
		case 0x04:
			if (ArcadeJoystick==1)
				return ((stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_A) && (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 1 : 0);
			else
				return ((stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && (stateJoy1.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0);
					
							 
		
		//case 0x2F: return ((stateJoy1.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0);			 	
		case 0x21: return ((stateJoy1.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0);			 
		// Fire buttons
 
	}

	if (dwResultP2 == ERROR_SUCCESS)
	{
		switch (nCode)
		{
			case 0x07:
				return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? 1 : 0);
			case 0x03:
				return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_START ? 1 : 0);
			case 0x4002: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP    ? 1 : 0 || stateJoy2.Gamepad.sThumbLY >  DEADZONE);		// Up
			case 0x4003: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? 1 : 0 || stateJoy2.Gamepad.sThumbLY < -DEADZONE);		// Down
			case 0x4000: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? 1 : 0 || stateJoy2.Gamepad.sThumbLX < -DEADZONE);		// Left
			case 0x4001: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 1 : 0 || stateJoy2.Gamepad.sThumbLX >  DEADZONE);		// Right


			case 0x4080: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_A ? 1 : 0);				// fire 1			
			case 0x4081: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_B ? 1 : 0);				// fire 2			
			case 0x4082: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_X ? 1 : 0);				// fire 3			
			case 0x4083: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_Y ? 1 : 0);				// fire 4		
			case 0x4084: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 1 : 0);			// fire 5			
			case 0x4085: return (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 1 : 0);			// fire 6	

			case 0x4088:
				return ( stateJoy2.Gamepad.bLeftTrigger > 128 ? 1 : 0);
			case 0x408A:			 
				return ( stateJoy2.Gamepad.bRightTrigger > 128 ? 1 : 0);

			case 0x403b:
				return ( stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB ? 1 : 0);
			case 0x403c:
				return ( stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB  ? 1 : 0);
			case 0x4004:
			if (ArcadeJoystick==1)
				return ((stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_A) && (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 1 : 0);
			else
				return ((stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && (stateJoy2.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0);

			//case 0x4021: return ((stateJoy2.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0);			 
		}
	}

	if (dwResultP3 == ERROR_SUCCESS)
	{
		switch (nCode)
		{
			case 0x08:
				return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? 1 : 0);
			case 0x04:
				return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_START ? 1 : 0);
			case 0x4102: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP    ? 1 : 0 || stateJoy3.Gamepad.sThumbLY >  DEADZONE);		// Up
			case 0x4103: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? 1 : 0 || stateJoy3.Gamepad.sThumbLY < -DEADZONE);		// Down
			case 0x4100: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? 1 : 0 || stateJoy3.Gamepad.sThumbLX < -DEADZONE);		// Left
			case 0x4101: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 1 : 0 || stateJoy3.Gamepad.sThumbLX >  DEADZONE);		// Right


			case 0x4180: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_A ? 1 : 0);				// fire 1			
			case 0x4181: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_B ? 1 : 0);				// fire 2			
			case 0x4182: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_X ? 1 : 0);				// fire 3			
			case 0x4183: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_Y ? 1 : 0);				// fire 4		
			case 0x4184: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 1 : 0);			// fire 5			
			case 0x4185: return (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 1 : 0);			// fire 6	

			case 0x4188:
				return ( stateJoy3.Gamepad.bLeftTrigger > 128 ? 1 : 0);
			case 0x418A:			 
				return ( stateJoy3.Gamepad.bRightTrigger > 128 ? 1 : 0);

			case 0x413b:
				return ( stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB  ? 1 : 0);
			case 0x413c:
				return ( stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB  ? 1 : 0);
			case 0x4104:
				if (ArcadeJoystick==1)
					return ((stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_A) && (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 1 : 0);
				else
					return ((stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && (stateJoy3.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0);

		}
	}

	if (dwResultP4 == ERROR_SUCCESS)
	{
		switch (nCode)
		{
			case 0x09:
				return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? 1 : 0);
			case 0x05:
				return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_START ? 1 : 0);
			case 0x4202: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP    ? 1 : 0 || stateJoy4.Gamepad.sThumbLY >  DEADZONE);		// Up
			case 0x4203: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? 1 : 0 || stateJoy4.Gamepad.sThumbLY < -DEADZONE);		// Down
			case 0x4200: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? 1 : 0 || stateJoy4.Gamepad.sThumbLX < -DEADZONE);		// Left
			case 0x4201: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 1 : 0 || stateJoy4.Gamepad.sThumbLX >  DEADZONE);		// Right


			case 0x4280: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_A ? 1 : 0);				// fire 1			
			case 0x4281: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_B ? 1 : 0);				// fire 2			
			case 0x4282: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_X ? 1 : 0);				// fire 3			
			case 0x4283: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_Y ? 1 : 0);				// fire 4		
			case 0x4284: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 1 : 0);			// fire 5			
			case 0x4285: return (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 1 : 0);			// fire 6	

			case 0x4288:
				return ( stateJoy4.Gamepad.bLeftTrigger > 128 ? 1 : 0);
			case 0x428A:			 
				return ( stateJoy4.Gamepad.bRightTrigger > 128 ? 1 : 0);

			case 0x423b:
				return ( stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB  ? 1 : 0);
			case 0x423c:
				return ( stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB  ? 1 : 0);
			case 0x4204:
				if (ArcadeJoystick==1)
					return ((stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_A) && (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 1 : 0);
				else
					return ((stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && (stateJoy4.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0);

		}
	}


	return 0;

}

// Read one joystick axis
int XInput2JoyAxis(int i, int nAxis)
{
	DWORD dwResultP1;
	
	XINPUT_STATE stateJoy1;
	  
    // Simply get the state of the controller from XInput.
	
    dwResultP1 = XInputGetState( 0, &stateJoy1 );

	if(nAxis == 1)
		return ((stateJoy1.Gamepad.sThumbLY/16384 ) * -1);
	else 
		return (stateJoy1.Gamepad.sThumbLX/16384);
 
}

// Read one mouse axis
int XInput2MouseAxis(int i, int nAxis)
{

	DWORD dwResultP1;
	

	XINPUT_STATE stateJoy1;
	 
 
    // Simply get the state of the controller from XInput.
	
    dwResultP1 = XInputGetState( 0, &stateJoy1 );

	if(nAxis == 1)
		return ((stateJoy1.Gamepad.sThumbLY/16384 ) * -1);
	else 
		return (stateJoy1.Gamepad.sThumbLX/16384);
	 
}

// This function finds which key is pressed
// To ensure analog joystick axes are handled correctly, call with CreateBaseline = true the 1st time
int XInput2Find(bool CreateBaseline)
{
 
	return 0;
}

int XInput2GetControlName(int nCode, TCHAR* pszDeviceName, TCHAR* pszControlName)
{


	return 0;
}

// ----------------------------------------------------------------------------

struct InputInOut InputInOutXInput2 = { XInput2Init, XInput2Exit, XInput2SetCooperativeLevel, XInput2Start, XInput2State, XInput2JoyAxis, XInput2MouseAxis, XInput2Find, XInput2GetControlName, NULL };
