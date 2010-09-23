/* lstnMT01.c gets MT01 responses */

#include <stdio.h>
#include <errno.h>
#define __USE_GNU
#include <string.h>
#include <sys/select.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

#define BBUFFSIZE 8192 

extern int ifd, kfd;

static unsigned char bBuff[BBUFFSIZE];

int
lstnMT01(char *valid_str){
//returns 0 if successful finding string
	struct timeval tv;
	int len, totlen;
	fd_set rfds;
	int i;	
	totlen = 0;
  
	while(totlen<BBUFFSIZE-1){
		FD_ZERO (&rfds);    //if I don't reset all of these, then in the next
		FD_SET (ifd, &rfds);    //call to select, only whichever fd that was set
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		if (select(ifd + 1,&rfds,NULL,NULL, &tv) < 0) {
			if (errno != EINTR)
				break;
		}
		else if (FD_ISSET(ifd, &rfds)) {
			// nap for a bit to give the instrument a chance to send a few more characters
			nap (0.05);
			if ((len = readDebug (ifd, &bBuff[totlen], sizeof(bBuff)-totlen-1, "instrument")) < 0) {
				perror("read");
				restore_tty(kfd);
				return (2);
			}
			bBuff[totlen + len] = '\0';
			totlen += len;
			
		}
		else { //timeout after MT01 is done spitting userprompt i
			break;
		}
	}
	if (totlen == 0) {
		Debug("LstnMT01:No bytes read in from serial port, check cable connection.\n"); 
		return 4;  
	}
	// Replace any nulls within the buffer
	for (i = 0; i < totlen; i++) {
		if (!bBuff[i])
		bBuff[i] = ' ';
	}
	DebugBuf(totlen, bBuff, "MT01 capture tmp buffer\n");
	log_printf(LOG_SCREEN_MAX, (char *) bBuff);
	if(valid_str && !(strcasestr((char *) bBuff,valid_str))) {
		Debug ("Cannot find %s in MT01 capture tmp buffer\n", valid_str);
		return 3;       
	}

	return 0;
}

unsigned char *
get_response_buf(void) {
	return bBuff;
}
