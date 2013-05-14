/* ****************************************************************************	*
 * This file is part of the Minerva Technology MT01 serial to compactflash	*
 * logger firmware (MT01 Firmware).						*
 *										*
 * MT01 Firmware is free software; you can redistribute it and/or modify	*
 * it under the terms of the GNU General Public License as published by		*
 * the Free Software Foundation; either version 2 of the License, or		*
 * (at your option) any later version.						*
 *										*
 * MT01 Firmware is distributed in the hope that it will be useful,		*
 * but WITHOUT ANY WARRANTY; without even the implied warranty of		*
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		*
 * GNU General Public License for more details.					*
 *										*
 * You should have received a copy of the GNU General Public License		*
 * along with the MT01 Firmware; if not, write to the Free Software		*
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	*
 * **************************************************************************** */

/* (C) Copyright 2004
 * Jonathan Schmidt, Minerva Technology Inc.,	jon@minervatech.net		*/

#include "stdio.h"
#include "crc.h"

unsigned short cal_crc( unsigned char *ptr, int count)
{
	int i;
	register unsigned short crc;

	crc = 0;
	while ( count-- > 0 )
	{
		crc = crc ^ (unsigned short) *ptr++ << 8;

		i = 8;	
		do
		{
			if ( crc & 0x8000 )
				crc = crc << 1 ^ XMODEM_POLY;
			else
				crc = crc << 1;
		} while (--i);
	}
	//printf("crc is %04X\n\r",crc);
	return crc;
}
/*
void iterative_crc( u16 * working_crc, u08 data)
{
	u08 i;
	register u16 crc;

	crc = *working_crc;

	crc = crc ^ (u16) data << 8;

	i = 8;	
	do
	{
		if ( crc & 0x8000 )
			crc = crc << 1 ^ XMODEM_POLY;
		else
			crc = crc << 1;
	} while (--i);

	*working_crc = crc;
}

void cal_checksum( u08 * checksum, u08 * ptr, u08 count )
{
	*checksum = 0;

	while( count-- > 0 )
		*checksum += *(ptr++);
}
*/
