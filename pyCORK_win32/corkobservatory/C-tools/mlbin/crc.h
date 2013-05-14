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



#ifndef	CRC_H
#define	CRC_H

#define XMODEM_POLY	0x1021  //CCITT
//#define XMODEM_POLY	0x8005  //CRC16


unsigned short cal_crc( unsigned char * ptr, int count);
//void iterative_crc( u16 * working_crc, u08 data);
//void cal_checksum( u08 * checksum, u08 * ptr, u08 count );

#endif // CRC_H
