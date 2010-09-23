#include <stdio.h>

#include "mltermfuncs.h"
#include "specialkeys.h"
//#include "usermenu.h"

int isspecialkey(unsigned char *cc, int len){
//if changes are made here, you must also change help.c	
//strchr ("\x1B\x02\x04\x05\x06\x08\x10\x11\x14\x15\x1A\x42\x48", cc[0])
if (cc[0] == 0x13)  // ^s entered
	return MT01_MENU;
else if (cc[0] == CR)
	return RETURN;
else if (cc[0] == '!')
	return STORE;
else if (cc[0] == STX) //ctrl-b baud change requested
	return BAUD_RATE;
else if ((cc[0] == ESC) && (len == 1))  //ESC or ctrl-q
	return QUITLOW;
else if (((cc[0] == ESC) && (len == 2) && (cc[1] == 0x6C)) || (cc[0] == 0x05))  //alt-l or ctrl-e
	return LINE_EDIT;
//else if (((cc[0] == ESC) && (len == 2) && (cc[1] == 0x31)) || (cc[0] == 0x06)
//	|| ((cc[0] == ESC) && (len == 3) && (cc[1] == 0x4F) && (cc[2] == 0x51))
//	|| ((cc[0] == ESC) && (len == 4) && (cc[1] == 0x5B) && (cc[2] == 0x5B) && (cc[3] == 0x42)))  //alt-1, ctrl-f, or F2
//	return OPEN_LOG;
else if (((cc[0] == ESC) && (len == 2) && (cc[1] == 0x32)) || (cc[0] == 'H'))  //alt-2 or H
	return HELP_MENU;
else if (((cc[0] == ESC) && (len == 2) && (cc[1] == 0x62)) || (cc[0] == 'B'))  //alt-b or B
	return SHOW_BAUD;
else if (((cc[0] == ESC) && (len == 3) && (cc[1] == 0x4F) && (cc[2] == 0x53)) || (cc[0] == 0x15) 
	|| ((cc[0] == ESC) && (len == 4) && (cc[1] == 0x5B) && (cc[2] == 0x5B) && (cc[3] == 0x44)))  //F4 or ctrl-u
	return FORM_DATALOW;
else if (((cc[0] == ESC) && (len == 3) && (cc[1] == 0x4F) && (cc[2] == 0x50)) || (cc[0] == 0x01)  
	|| ((cc[0] == ESC) && (len == 4) && (cc[1] == 0x5B) && (cc[2] == 0x5B) && (cc[3] == 0x41)))  //F1 or ctrl-a
	return FORM_TIME;
else if (((cc[0] == ESC) && (len == 4) && (cc[1] == 0x5B) && (cc[2] == 0x36) && (cc[3] == 0x7E))
	|| (cc[0] == 0x04))  //PgDn or ctrl-d
	return DOWNLOADLOW;
else if ((cc[0] == ESC && len == 4 && cc[1] == 0x5B && cc[2] == 0x5B && cc[3] == 0x45) || cc[0] == 0x19
	|| (cc[0] == ESC && len == 5 && cc[1] == 0x5B && cc[2] == 0x31 && cc[3] == 0x35 && cc[4] == 0x7E))  // F5 or ctrl-y
	return SET_TIMELOW;
else if ((cc[0] == ESC && len == 5 && cc[1] == 0x5B && cc[2] == 0x32 && (cc[3] == 0x31 || cc[3] == 0x33) && cc[4] == 0x7E)
	|| cc[0] == 0x07)   //F10, F11 or ctrl-g
	return PAR_DNLOADLOW;
else if (cc[0] == 0x0F) //ctrl-o
	return PAR_UPLOADLOW;
else if (cc[0] == 0x0E) //ctrl-n
	return NAKEDROM;
else if (cc[0] == 0x1A) //ctrl-z
	return PAUSE;
else if (cc[0] == 0x17) //ctrl-w
	return WAKE;
else
	return 0;

}
