/************************************************************

    DALLAS DS1302

    RTC + BACKUP RAM

    Emulation by ElSemi

    Missing Features:
      - Burst Mode
      - Clock programming (useless)

    2009-05 Converted to be a device

    Ported 30/1/2012 - MAME 0.144u7

************************************************************/

#include "burnint.h"

static INT32 shift_in = 0;
static UINT8 shift_out = 0;
static UINT8 icount = 0;
static UINT8 last_clk = 0;
static UINT8 last_cmd = 0;
static UINT8 sram[0x20];

/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

INLINE UINT8 convert_to_bcd(int val)
{
	return ((val / 10) << 4) | (val % 10);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void ds1302_reset()
{
	memset (sram, 0, 0x20);

	shift_in  = 0;
	shift_out = 0;
	icount    = 0;
	last_clk  = 0;
	last_cmd  = 0;
}

/*-------------------------------------------------
    ds1302_dat_write
-------------------------------------------------*/

void ds1302_dat_write(UINT8 data)
{
	if (data)
	{
		shift_in |=  (1 << icount);
	}
	else
	{
		shift_in &= ~(1 << icount);
	}
}

/*-------------------------------------------------
    ds1302_clk_write
-------------------------------------------------*/

void ds1302_clk_write(UINT8 data)
{
	if (data != last_clk)
	{
		if (data)	// Rising, shift in command
		{
			icount++;

			if (icount == 8)	// Command start
			{
				time_t nLocalTime = time(NULL);
				tm* tmLocalTime = localtime(&nLocalTime);

				switch (shift_in)
				{
					case 0x81:	// Sec
						shift_out = convert_to_bcd(tmLocalTime->tm_sec);
						break;

					case 0x83:	// Min
						shift_out = convert_to_bcd(tmLocalTime->tm_min);
						break;

					case 0x85:	// Hour
						shift_out = convert_to_bcd(tmLocalTime->tm_hour);
						break;

					case 0x87:	// Day
						shift_out = convert_to_bcd(tmLocalTime->tm_mday);
						break;

					case 0x89:	// Month
						shift_out = convert_to_bcd(tmLocalTime->tm_mon + 1);
						break;

					case 0x8b:	// weekday
						shift_out = convert_to_bcd(tmLocalTime->tm_wday);
						break;

					case 0x8d:	// Year
						shift_out = convert_to_bcd(tmLocalTime->tm_year % 100);
						break;

					default:
						shift_out = 0x0;
				}

				if (shift_in > 0xc0)
				{
					shift_out = sram[(shift_in >> 1) & 0x1f];
				}

				last_cmd = shift_in & 0xff;
				icount++;
			}

			if (icount == 17 && !(last_cmd & 1))
			{
				UINT8 val = (shift_in >> 9) & 0xff;

				switch (last_cmd)
				{
					case 0x80:	// Sec
						break;

					case 0x82:	// Min
						break;

					case 0x84:	// Hour
						break;

					case 0x86:	// Day
						break;

					case 0x88:	// Month
						break;

					case 0x8a:	// weekday
						break;

					case 0x8c:	// Year
						break;

					default:
						shift_out = 0x0;
				}

				if (last_cmd > 0xc0)
				{
					sram[(last_cmd >> 1) & 0x1f] = val;
				}
			}
		}
	}

	last_clk = data;
}

/*-------------------------------------------------
    ds1302_read
-------------------------------------------------*/

UINT8 ds1302_read()
{
	return (shift_out & (1 << (icount - 9))) ? 1 : 0;
}

/*-------------------------------------------------
    ds1302_scan
-------------------------------------------------*/

INT32 ds1302Scan()
{
	for (INT32 i = 0; i < 32; i++) {
		SCAN_VAR(sram[i]);
	}

	SCAN_VAR(icount);
	SCAN_VAR(shift_in);
	SCAN_VAR(shift_out);
	SCAN_VAR(last_clk);
	SCAN_VAR(last_cmd);

 	return 0;
}
