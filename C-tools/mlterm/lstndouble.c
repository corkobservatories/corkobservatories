/* lstn.c gets RTC/PPC responses */
//consider putting select loop in to look for ESC character from user to break out?

#include <stdio.h>
#include <errno.h>
#include <sys/select.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

#define BBUFFSIZE 8192 
extern int ifd, kfd;

int lstndouble(unsigned char *bold, unsigned char *bnew){

	struct timeval tv;
	int i,j, len, totlen;
	fd_set rfds;
	unsigned char bBuff[BBUFFSIZE];
	unsigned char *bz;
	
	totlen = 0;
    while(totlen<BBUFFSIZE-1){
		FD_ZERO(&rfds);
		FD_SET(ifd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		if (select(ifd+1,&rfds,NULL,NULL, &tv) < 0) { //removed select on keyboard. Don't want to allow quit.
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
			log_printf(LOG_SCREEN_MAX, (char *) &bBuff[totlen]);
			totlen += len;
			
		}
		else { //if timeout exit only
			break;
		}
	}
	if (totlen == 0) {
		Debug("lstndouble: No bytes read in from serial port, check cable connection.\n");
		return 4;  // !E or !PPE eerom line should be echoed
	}
	DebugBuf(totlen, bBuff, "EEROM capture tmp buffer\n");
	for(i=0;i<totlen;i++){
		if(bBuff[i]=='E' && bBuff[i+24] == 'E'){
			if(bBuff[i+1]=='0' && bBuff[i+25] == '1'){
				if(bBuff[i+2]==' ' && bBuff[i+26] == ' ') break;
			}
		}
	}
	if (i == totlen) {
		Debug ("Cannot find EO in EEROM capture tmp buffer\n");
		return 3;        // can't find "E0 "
	}

	bz =  bold + 128;
	while(i < totlen && bz-bold >= 8){
		//Debug ("i = %d\n",i);
		i += 3;
		for(j=0; j<4; j++){
			*bold++ = hex2int((char *) &bBuff[i]);
			i += 2;
			*bold++ = hex2int((char *) &bBuff[i]);
			i += 3;
		}
		i++;
	}
	bz = bnew + 128;
	while(i < totlen && bz-bnew >= 8){
		//Debug ("i = %d\n",i);
		i += 3;
		for(j=0; j<4; j++){
			*bnew++ = hex2int((char *) &bBuff[i]);
			i += 2;
			*bnew++ = hex2int((char *) &bBuff[i]);
			i += 3;
		}
		i++;
	}
	if (bz-bnew) {  // returns true if less than 128 bytes or >=136 bytes.
		Debug("Incomplete EEROM transfer, check cable connection.\n");
		return 4;
	}

return 0;
}
