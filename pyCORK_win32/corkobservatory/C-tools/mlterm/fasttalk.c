#include <stdio.h>
#include <string.h>

#include "mltermfuncs.h"

extern int ifd;

void fasttalk(unsigned char *cmd){

	struct timeval tv;

	/* send command to comport after delay */
	nap (0.05);
	if (cmd) {
		writeDebug(ifd,cmd,strlen((char *) cmd),"instrument");
		log_printf(LOG_SCREEN_MAX, (char *) cmd);
	}
	/* send command to comport after delay */
}
