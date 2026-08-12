/* JOYSTICK.C - joystick support for WinX68k */

#include "common.h"
#include "prop.h"
#include "joystick.h"
#include "winui.h"
#include "keyboard.h"

#include "libretro.h"
extern retro_input_state_t input_state_cb;
extern uint32_t libretro_supports_input_bitmasks;

uint8_t JoyKeyState;
static uint8_t JoyKeyState0;
static uint8_t JoyKeyState1;

static uint8_t AnalogState[3];
static uint8_t JoyState[2][2];
static uint8_t JoyPortData[2];

/* This stores whether the buttons were down. This avoids key repeats. */
static uint8_t JoyDownState0;

#define CYBER_LATENCY 3
static int CyberCount = 0;
static int CyberLatency = CYBER_LATENCY;

static uint8_t JoyCyberA_Read(void)
{
   uint8_t ret = 0xff;

   switch (CyberCount)
   {
   case 0:
      /* data start - waits a bit after REQ */
      /* Suppose to take about 50us to respond, using counters here instead */
      ret = 0xff;
      break;
   case 1:
      /* data0: button A, B, C, D */
      ret = JoyState[0][0];
      break;
   case 2:
      /* data1: button E1, E2, F (Start), G (Select) */
      ret = JoyState[0][1];
      break;
   case 3:
      /* data2: Y axis, b4-b7 */
      ret = 0x90;
      ret |= (AnalogState[1] >> 4) & 0x0f;
      break;
   case 4:
      /* data3: X axis, b4-b7 */
      ret = 0xb0;
      ret |= (AnalogState[0] >> 4) & 0x0f;
      break;
   case 5:
      /* data4: Throttle Up/Down, b4-b7 */
      ret = 0x90;
      ret |= (AnalogState[2] >> 4) & 0x0f;
      break;
   case 6:
      /* data5: reserved */
      ret = 0xb0;
      break;
   case 7:
      /* data6: Y axis, b0-b3 */
      ret = 0x90;
      ret |= AnalogState[1] & 0x0f;
      break;
   case 8:
      /* data7: X axis, b0-b3 */
      ret = 0xb0;
      ret |= AnalogState[0] & 0x0f;
      break;
   case 9:
      /* data8: Throttle Up/Down, b0-b3 */
      ret = 0x90;
      ret |= AnalogState[2] & 0x0f;
      break;
   case 10:
      /* data9: reserved */
      ret = 0xb0;
      break;
   case 11:
      /* data10: mini buttons A/B on lever */
      ret = 0x9f & ~((~JoyState[0][0] & 0x0c));
      break;
   case 12:
      /* data11: */
      ret = 0xbf;
      break;
   default:
      ret = 0xff;
      break;
   }

   /* decrement internal counter */
   CyberLatency--;

/*   log_cb(RETRO_LOG_DEBUG, "lat = %2d count = %d ret = %02x\n", CyberLatency, CyberCount, ret); */

   if (CyberLatency <= 0)
   {
      if (CyberCount >= 13)
      {
         ret = 0xff;
      }
      else
      {
         /* next data cycle */
         CyberCount++;

         /* reset internal counter */
         CyberLatency = CYBER_LATENCY;
      }
   }

   return ret;
}

static void JoyCyberA_Write(uint8_t data)
{
   if ((data == 0) && (JoyPortData[0] == 0xff))
   {
      /* reset data acquisition cycle */
      CyberCount = 0;

      /* initialize internal latenct */
      CyberLatency = CYBER_LATENCY;
   }
   JoyPortData[0] = data;
}

void Joystick_Init(void)
{
   JoyKeyState = 0;
   JoyKeyState0 = 0;
   JoyKeyState1 = 0;
   JoyState[0][0] = 0xff;
   JoyState[0][1] = 0xff;
   JoyState[1][0] = 0xff;
   JoyState[1][1] = 0xff;
   JoyPortData[0] = 0;
   JoyPortData[1] = 0;
   AnalogState[0] = 0x97;
   AnalogState[1] = 0xb7;
   AnalogState[2] = 0x97;
   CyberCount     = 0;
   CyberLatency   = CYBER_LATENCY;
}

void Joystick_Cleanup(void)
{
}

uint8_t FASTCALL Joystick_Read(uint8_t num)
{
	uint8_t ret0 = 0xff, ret1 = 0xff;
   uint8_t ret = 0;

   switch (Config.JOY_TYPE[num])
   {
   case PAD_CYBERSTICK_A:
      if (num == 0)
         return JoyCyberA_Read();
      return 0xff;

   case PAD_CYBERSTICK_D:
   case PAD_CPSF_MD:
   case PAD_CPSF_SFC:
      if (JoyPortData[num] == 0xff)
         return JoyState[num][1];
      return JoyState[num][0];

   case PAD_2BUTTON:
      if (JoyPortData[num] == 0xff)
         return 0xff;
      return JoyState[num][0];
   }
   return 0xff;
}

void FASTCALL Joystick_Write(uint8_t num, uint8_t data)
{
	switch (Config.JOY_TYPE[num])
   {
   case PAD_CYBERSTICK_A:
      if (num == 0)
         JoyCyberA_Write(data);
      else
         JoyPortData[num] = data;
      break;

   case PAD_CYBERSTICK_D:
   case PAD_2BUTTON:
   case PAD_CPSF_MD:
   case PAD_CPSF_SFC:
      JoyPortData[num] = data;
      break;
   }
}

/* Menu navigation related vars */
#define RATE   3      /* repeat rate */
#define JOYDELAY 30   /* delay before 1st repeat */

static uint32_t get_px68k_input_bitmasks(int port)
{
   return input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
}

static uint16_t get_px68k_input(int port)
{
   uint32_t i   = 0;
   uint32_t res = 0;
   for (i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R3; i++)
      res |= input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
   return res;
}

void FASTCALL Joystick_Update(int is_menu, int key, int port)
{
   static uint8_t pre_ret0 = 0xff;
   uint8_t ret0            = 0xff, ret1 = 0xff;
   uint16_t temp           = 0;
   uint32_t res            = 0;
   uint8_t *p_JoyState     = JoyState[port];

   if (libretro_supports_input_bitmasks)
      res                  = get_px68k_input_bitmasks(port);
   else
      res                  = get_px68k_input(port);

   /* D-Pad */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      temp |= JOY_RIGHT;
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      temp |=  JOY_LEFT;
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      temp |=  JOY_UP;
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      temp |=  JOY_DOWN;

   /* ignore UP/DOWN, LEFT/RIGHT button combination */
   if ((temp & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
      temp &= ~(JOY_LEFT | JOY_RIGHT);
   if ((temp & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
      temp &= ~(JOY_UP | JOY_DOWN);

   /* Trigger 1 */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      temp |= (Config.VbtnSwap ? JOY_TRG1 : JOY_TRG2);
   /* Trigger 2 */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
      temp |= (Config.VbtnSwap ? JOY_TRG2 : JOY_TRG1);
   /* Trigger 1 - alternate key */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
      temp |= (Config.VbtnSwap ? JOY_TRG2 : JOY_TRG1);
   /* Trigger 2 - alternate key */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
      temp |= (Config.VbtnSwap ? JOY_TRG1 : JOY_TRG2);

   /* Start */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
      temp |= ~(JOY_UP | JOY_DOWN);
   /* Select */
   if (!Config.joy1_select_mapping)
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
         temp |= ~(JOY_LEFT | JOY_RIGHT);

   /* ret0 is for menu handling */
   ret0 ^= temp;

   p_JoyState[0] = 0xff;
   p_JoyState[1] = 0xff;

   /* Buttons */
   switch (Config.JOY_TYPE[port])
   {
      case PAD_2BUTTON:
         p_JoyState[0] = ret0;
         break;

      case PAD_CPSF_MD:
         /* data[0]: dpad, A, B */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            p_JoyState[0] &= ~JOY_RIGHT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            p_JoyState[0] &= ~JOY_LEFT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            p_JoyState[0] &= ~JOY_UP;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            p_JoyState[0] &= ~JOY_DOWN;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
            p_JoyState[0] &= ~JOY_TRG1;	/* Low-Kick */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            p_JoyState[0] &= ~JOY_TRG2;	/* Mid-Kick */
         /* data[1]: C, X, Y, Z, Start, Mode */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
            p_JoyState[1] &= ~JOY_TRG4; 	/* Low-Punch */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
            p_JoyState[1] &= ~JOY_TRG3;	/* Mid-Punch */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_L))
            p_JoyState[1] &= ~JOY_TRG5;	/* High-Punch */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_R))
            p_JoyState[1] &= ~JOY_TRG8;	/* High-Kick */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            p_JoyState[1] &= ~JOY_TRG6; /* Start */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
            p_JoyState[1] &= ~JOY_TRG7;	/* Mode */
         break;

      case PAD_CPSF_SFC:
         /* data[0]: dpad, A, B */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            p_JoyState[0] &= ~JOY_RIGHT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            p_JoyState[0] &= ~JOY_LEFT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            p_JoyState[0] &= ~JOY_UP;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            p_JoyState[0] &= ~JOY_DOWN;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
            p_JoyState[0] &= ~JOY_TRG2;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            p_JoyState[0] &= ~JOY_TRG1;
         /* data[1]: X, Y, L, R, Start, Select */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
            p_JoyState[1] &= ~JOY_TRG3;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
            p_JoyState[1] &= ~JOY_TRG4;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_L))
            p_JoyState[1] &= ~JOY_TRG8;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_R))
            p_JoyState[1] &= ~JOY_TRG5;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            p_JoyState[1] &= ~JOY_TRG6;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
            p_JoyState[1] &= ~JOY_TRG7;
         break;

      case PAD_CYBERSTICK_D:
         /* data[0] Right, Left, Up, Down, A, B */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            p_JoyState[0] &= ~JOY_RIGHT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            p_JoyState[0] &= ~JOY_LEFT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            p_JoyState[0] &= ~JOY_UP;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            p_JoyState[0] &= ~JOY_DOWN;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
            p_JoyState[0] &= ~0x20;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            p_JoyState[0] &= ~0x40;
         /* data[1]: buttons C, D, E1, E2 */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
            p_JoyState[1] &= ~0x04;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
            p_JoyState[1] &= ~0x08;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_L))
            p_JoyState[1] &= ~0x20;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_R))
            p_JoyState[1] &= ~0x40;
         /* EXTRA: for compatiblity. not normally mapped on CyberStick Digital */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            p_JoyState[0] &= ~JOY_START;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
            p_JoyState[0] &= ~JOY_SELECT;
         break;

      case PAD_CYBERSTICK_A:
#if 0
		analog_z = input_state_cb(port, RETRO_DEVICE_ANALOG,
			RETRO_DEVICE_INDEX_ANALOG_BUTTON,
		    RETRO_DEVICE_ID_JOYPAD_L2);

		if (analog_z == 0)
		{
			if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
			                   RETRO_DEVICE_ID_JOYPAD_L2))
			{
				analog_z = 0x8000;
			}
			else
			{
				analog_z = 0;
			}
		}
#endif
		AnalogState[0] = (input_state_cb(port,
			RETRO_DEVICE_ANALOG,
			RETRO_DEVICE_INDEX_ANALOG_LEFT,
			RETRO_DEVICE_ID_ANALOG_X) + 0x8000) >> 8;
		AnalogState[1] = (input_state_cb(port,
			RETRO_DEVICE_ANALOG,
			RETRO_DEVICE_INDEX_ANALOG_LEFT,
			RETRO_DEVICE_ID_ANALOG_Y) + 0x8000) >> 8;
		AnalogState[2] = (-input_state_cb(port,
			RETRO_DEVICE_ANALOG,
			RETRO_DEVICE_INDEX_ANALOG_RIGHT,
			RETRO_DEVICE_ID_ANALOG_Y) + 0x8000) >> 8;
		if (AnalogState[1] < 30)
			ret0 &= ~JOY_UP;
		else if (AnalogState[1] > (255 - 30))
			ret0 &= ~JOY_DOWN;
		if (AnalogState[0] < 30)
			ret0 &= ~JOY_LEFT;
		else if (AnalogState[0] > (255 - 30))
			ret0 &= ~JOY_RIGHT;
		/* data[0]: buttons A, B, C, D */
		/* data[10]: mini buttons A, B on lever */
		p_JoyState[0]   = 0x9f;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
			p_JoyState[0] &= ~0x08;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
			p_JoyState[0] &= ~0x04;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
			p_JoyState[0] &= ~0x02;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
			p_JoyState[0] &= ~0x01;
		/* data[1]: buttons E1, E2, F (Start), G (Select) */
		p_JoyState[1] = 0xbf;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_L))
			p_JoyState[1] &= ~0x08;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_R))
			p_JoyState[1] &= ~0x04;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			p_JoyState[1] &= ~0x02;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
			p_JoyState[1] &= ~0x01;
      break;
   }

   JoyDownState0   = ~(ret0 ^ pre_ret0) | ret0;
   pre_ret0        = ret0;

   /* input overrides section during Menu mode for faster menu browsing
    * by pressing and holding key or button aka turbo mode */
   if (is_menu)
   {
      int i;
      static int repeat_rate, repeat_delay;
      static uint8_t last_inbuf;
      uint8_t joy_in = (ret0 ^ 0xff);
      uint8_t inbuf  = (joy_in | key);

      if ((inbuf & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
         inbuf &= ~(JOY_LEFT | JOY_RIGHT);
      if ((inbuf & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
         inbuf &= ~(JOY_UP | JOY_DOWN);

      if (last_inbuf != inbuf)
      {
         last_inbuf    = inbuf;
         repeat_delay  = JOYDELAY;
         repeat_rate   = 0;
         JoyDownState0 = (inbuf ^ 0xff);
      }
      else
      {
         if (repeat_delay)
            repeat_delay--;
         if (repeat_delay == 0)
         {
            if (repeat_rate)
               repeat_rate--;
            if (repeat_rate == 0)
            {
               repeat_rate = RATE;
               for (i = 0; i < 4; i++)
               {
                  uint8_t tmp = (1 << i); /* which direction? UP/DOWN/LEFT/RIGHT */
                  if ((inbuf & tmp) == tmp)
                     JoyDownState0 &= ~tmp;
               }
            }
         }
      }
   }
   else
   {
      /* disable Joystick when software keyboard is active */
      if (!Keyboard_IsSwKeyboard())
      {
#if 0
         JoyState[0][port] = ret0;
         JoyState[1][port] = ret1;
#endif
      }
   }
}

uint8_t get_joy_downstate(void)   { return JoyDownState0;  }
void    reset_joy_downstate(void) { JoyDownState0 = 0xff;  }
