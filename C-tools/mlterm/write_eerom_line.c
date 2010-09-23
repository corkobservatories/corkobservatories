#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <sys/select.h>

#include "mltermfuncs.h"
#include "specialkeys.h"

#define INTERVAL 2.0

extern int ifd,kfd,nfds;

int write_eerom_line(const unsigned char *line, int rtc_flag, int ppcnum, int addr) {    
//returns 0 if successful, -1 if not	
	int rtn;
	char out[36];
	float elapsed;
	float delay;
	static struct timeb now;
	static struct timeb previous = {0,0};

	struct timeval tv;
	fd_set rfds;
	char c[512];
	int len;

	tv.tv_sec=0;
	tv.tv_usec=0;
	FD_ZERO(&rfds);
	FD_SET(kfd,&rfds);
	//check for user entered ESC here. 
	if(select(nfds,&rfds,NULL, NULL, &tv) > 0) {
		if (FD_ISSET (kfd, &rfds)) {
			if ((len = readDebug(kfd, c, sizeof(c),"keyboard")) < 0) {
				perror("read");
				restore(1);
			}
			if (isspecialkey((unsigned char *) c, 1) == QUITLOW) { //used to be len instead of 1 to protect from function key causing escape.
																//however, the tendancy of an impatient user to hit esc more than once is high!
				Debug("User entered ESC.\n");
				log_printf(LOG_SCREEN_MAX, "User entered ESC.\n");
				return -1;
			}
		}
	}

	ftime (&now);
	elapsed = (now.time - previous.time) + (now.millitm - previous.millitm) / 1000.0;
	if ((delay = INTERVAL - elapsed) > 0.001)
		nap (delay);

	if (rtc_flag) {
		Debug("writing line E%X to RTC\n",addr); 
		Qtalk((unsigned char *) "!Q\r");
		log_printf(LOG_SCREEN_MAX, "!Q\n");
		sprintf(out, "!%02X%02X%02X%02X%02X%02X%02X%02X%02X\r", 0xE0+addr, line[0], line[1], line[2], line[3], line[4], line[5], line[6], line[7]);
		if(!checkROM(out, 0)) {
			rtn = send_line(out, 0);
			if(rtn) {
				Qtalk((unsigned char *) "!Q\r");
				slowtalk((unsigned char *) "!!\r");
				log_printf(LOG_SCREEN_MAX, "!Q\n!!\n");
				return rtn;
			}
		}
	} else {
		Debug("writing line E%X to PPC number %d\n", addr, ppcnum); 
		unsigned char ppcmd[8]; //one extra space for null terminator
		sprintf((char *) ppcmd, "!P%x\r", ppcnum);
		Qtalk((unsigned char *) "!Q\r");                   // stop RTC
		slowtalk(ppcmd);
		log_printf(LOG_SCREEN_MAX, "%s\n%s\n", "!Q", ppcmd);
		sprintf(out, "!PP%02X%02X%02X%02X%02X%02X%02X%02X%02X\r", 0xE0+addr, line[0], line[1], line[2], line[3], line[4], line[5], line[6], line[7]);
		if(!checkROM(out, 0)) {
			rtn = send_line(out, 0);
			if(rtn) {
				Qtalk((unsigned char *) "!Q\r");
				slowtalk((unsigned char *) "!!\r");
				log_printf(LOG_SCREEN_MAX, "!Q\n!!\n");
				return rtn;
			}
		}
	}
	//if (rtc_flag && addr == 0x0E) slowtalk("!S\r"); //commented out because chose to force user to use F5 or ctrl ^t in mlterm instead.
	slowtalk((unsigned char *) "\x03");  //ctrl^c to activate changes
	log_printf(LOG_SCREEN_MAX, "^c\n");
	ftime (&previous);
	return 0; 
}

int send_line(char *out, int flag) {
	//flag is 0 when want to check successful receipt of EEROM line by response of old and new eerom returned
	unsigned char bOld[128], bNew[128];
	int rtn;
	
	slowtalk((unsigned char *) out); 
	log_printf(LOG_SCREEN_MAX, "\nComplete line sent was %s\n", out);
	Debug("Complete line sent was %s\n",out);
	if (flag) return 0;
	if((rtn = lstndouble(bOld, bNew)) != 0) {
		if (rtn == 3) {
			Debug("No response from instrument.\n");
			log_printf(LOG_SCREEN_MIN, "No response from instrument.\n");
		}
		else if (rtn == 4) {
			Debug("Incomplete EEROM transmit, check cable connection.\n");
			log_printf(LOG_SCREEN_MIN, "Incomplete EEROM transmit, check cable connection.\n");
		} else if (rtn == 2) restore(2);
		return -1;
	}
	DebugBuf(128, bOld, "Old EEROM capture\n");
	DebugBuf(128, bNew, "New EEROM capture.\n");
	return 0;
}

int checkROM(char *in, int sbuflen) {    
	int i;
	char *linefeed;
	char *charret;

	linefeed = strchr(in,'\n');
	if(linefeed)
		*linefeed = '\0'; //replace newline with null
	charret = strchr(in,'\r');
	if(charret)
		*charret = '\0'; //replace return with null
	for(i=0;i<strlen(in);i++)
		in[i]=toupper(in[i]);
	
	if (!strncmp(in, "!E", 2)) {
		//check if second to last character is a newline (last character is '\0')and then there are 2 extra spaces in "in" array)
		if (strlen(in) != 19) { 
			Debug("Wrong length.\n");
			log_printf(LOG_SCREEN_MIN, "\nWrong length.");
			Debug("RTC EEROM Entry cancelled.\n");
			log_printf(LOG_SCREEN_MIN, "\nRTC EEROM Entry cancelled.\n");
			return 1;

		} else if (!ishexstring (&in[1])) {
			Debug("EEROM characters must be in hex.\n");
			log_printf(LOG_SCREEN_MIN, "\nEEROM characters must be in hex.");
			Debug("RTC EEROM Entry cancelled.\n");
			log_printf(LOG_SCREEN_MIN, "\nRTC EEROM Entry cancelled.\n");
			return 1;
		} else if (!strncmp(in,"!EF",3)) {
			Debug("Overwriting RTC EEROM line EF is blocked\n");
			log_printf(LOG_SCREEN_MIN, "\nOverwriting RTC EEROM line EF is blocked.");
			Debug("Entry cancelled.\n");
			log_printf(LOG_SCREEN_MIN, "\nEntry cancelled.\n");
			return 1;
		}
	} else if (!strncmp(in,"!PPE",4)){ 
		if (strlen(in) != 21) { 
			Debug("Wrong length.\n");
			log_printf(LOG_SCREEN_MIN, "\nWrong length.");
			Debug("PPC EEROM Entry cancelled.\n");
			log_printf(LOG_SCREEN_MIN, "\nPPC EEROM Entry cancelled.\n");
			return 1;
		} else if (!ishexstring (&in[3])) {
			Debug("EEROM characters must be in hex.\n");
			log_printf(LOG_SCREEN_MIN, "\nEEROM characters must be in hex.");
			Debug("RTC EEROM Entry cancelled.\n");
			log_printf(LOG_SCREEN_MIN, "\nRTC EEROM Entry cancelled.\n");
			return 1;
		} else if (!strncmp(in,"!PPEE",5)) {
			Debug("Overwriting PPC EEROM line PPEE is blocked\n");
			log_printf(LOG_SCREEN_MIN, "\nOverwriting PPC EEROM line PPEE is blocked.");
			Debug("Entry cancelled.\n");
			log_printf(LOG_SCREEN_MIN, "\nEntry cancelled.\n");
			return 1;
		}
	} 
	else {
		log_printf(LOG_SCREEN_MIN, "Not a valid EEROM address. Entry cancelled. \n");
		return 1;
	}
	strcat(in, "\r");// adds necessary carraige return
	return 0;
}

