/* lstn.c gets RTC/PPC responses */

#include <stdio.h>
#include <errno.h>
#include <sys/select.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "specialkeys.h"
//#include "usermenu.h"

#define BBUFFSIZE 8192 

extern int ifd, kfd, nfds;

int
lstn(unsigned char *b){

	struct timeval tv;
	int i,j, len, totlen;
	fd_set rfds;
	unsigned char bBuff[BBUFFSIZE];
	unsigned char cc[1024];
	unsigned char *bz;
	
	totlen = 0;
    while(totlen<BBUFFSIZE-1){
		FD_ZERO (&rfds);    //if I don't reset all of these, then in the next
		FD_SET (ifd, &rfds);    //call to select, only whichever fd that was set
		FD_SET (kfd, &rfds);    // to true in   the last call to select
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		if (select(nfds,&rfds,NULL,NULL, &tv) < 0) {
			if (errno != EINTR)
				break;
		}
		else if (FD_ISSET (kfd, &rfds)) {
			if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
				perror ("read");
				restore_tty (kfd);
				return (1);
			}
			if (isspecialkey(cc, 1) == QUITLOW) { //used to be len instead of 1 to protect from function key causing escape.
												//however, the tendancy of an impatient user to hit esc more than once is high!
				Debug("Read EEROM aborted.\n");
				log_printf(LOG_SCREEN_MAX, "User entered quit char %02X \n", cc[0]);
				return (27);
			}
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
		else { //timeout after PPC is done spitting EEROM data, and nothing happens for a half second.
			break;
		}
	}
	if (totlen == 0) {
		Debug("No bytes read in from serial port, check cable connection.\n"); 
		return 4;  // !D or !PPD should be echoed
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
		Debug ("Cannot find EO and E1 in EEROM capture tmp buffer\n");
		return 3;       // can't find "E0"	this is what happens when 
						// there is no PPC electrically connected
						// or the delay in sending automated commands is not long enough.
	}

	bz =  b + 128;

	while(i < totlen && bz-b >= 8){
		Debug ("i = %d\n",i);
		i += 3;
		for(j=0; j<4; j++){
			*b++ = hex2int((char *) &bBuff[i]);
			i += 2;
			*b++ = hex2int((char *) &bBuff[i]);
			i += 3;
		}
		i++;
	}
	if (bz-b) {
		Debug ("EEROM transmit not complete, check cable connection.\n");
		return 4; //if less than 128 bytes read since initial E0 byte
	}

	return 0;
}
