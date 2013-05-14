#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "usermenu.h"

#define SIZE 14 //user input array length: YYMMDDhhmmss\n\0

extern int kfd, ifd;


void settime(int high){
	static const char *time_format = "!EE14%0.2X%0.2X%0.2X%0.2X%0.2X%0.2X00\r";
    char in[SIZE],out[25];
	unsigned char ch;
	int Y,M,D,h,m,s;
	int i;
	struct timeb now;
	struct tm *tm;
	long offset;

	Debug("settime\n");
	if (!high) {
		log_printf(LOG_SCREEN_MIN, "\nAre both Pass Lstn and Pass Talk set to ON? (y/n)");
		readDebug(kfd, &ch, 1, "keyboard");
		Debug("user answered %c\n", ch);
		log_printf(LOG_SCREEN_MIN, "%c\n", ch);
		ch = toupper (ch);
		if (ch != 'Y') {
			log_printf(LOG_SCREEN_MIN, "\nExiting time entry.\n");
			Debug ("Exiting time entry...\n");
			return;
		}
		log_printf(LOG_SCREEN_MIN, "\nSynchronize time with computer clock? (y/n)");
		readDebug(kfd, &ch, 1, "keyboard");
		Debug("user answered %c\n", ch);
		log_printf(LOG_SCREEN_MIN, "%c\n", ch);
		ch = toupper (ch);
	}
	else {
		if (!confirm("sychronize instrument time")) return;
		//if (ChangeBaud("19200") == 0 && TalkLstn(0) == 0)
		log_printf(LOG_SCREEN_MIN, "\nPlease be patient...\n");
		if (TalkLstn(0) == 0)
			ch = 'Y';
	}
	slowtalk((unsigned char *) "Q");
	slowtalk((unsigned char *) "!Q\r");  //the only reason why this isn't Qtalk is because we nap for a number of seconds following
	if (ch == 'Y') { // FOR SYNCH FROM SYSTEM CLOCK
		ftime (&now);
		now.time += 4;
		nap(2.0 - now.millitm/1000.0);
		tm = gmtime (&now.time);
		sprintf(out,time_format, tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	} else if (ch == 'N') { //user enters time , this will never happen if high
		log_printf(LOG_SCREEN_MIN, "\nEnter time as YYMMDDhhmmss     ");
		restore_tty(kfd);
		termln(in, sizeof(in));
		raw(kfd,0);
		DebugBuf(strlen(in), (unsigned char *) in,"Read time from user\n");
		if (strchr(in,'\n') != &in[SIZE-2]){
			log_printf(LOG_SCREEN_MIN, "\nError, time entry cancelled.\n");
			return;
		}	
		in[SIZE-2] = '\0';
		i = sscanf(in,"%2d%2d%2d%2d%2d%2d",&Y,&M,&D,&h,&m,&s);
		log_printf(LOG_SCREEN_MIN, "%.2d%.2d%.2d%.2d%.2d%.2d\n",Y,M,D,h,m,s);
		if ( i!=6 || 
			Y>15 || M==0 || M>12 || D==0 || D>31 || h>23 || m>59 || s>59 ||
			( (M==2)&&(D>29) ) ||
			( (M==2) && (Y % 4!=0) && (D==29) ) ||
			( (M==4 || M==6 || M==9 || M==11) && (D>30) ) ){
			log_printf(LOG_SCREEN_MIN, "\nInvalid value...cancelled.\n");
			return;
		}
		sprintf(out,time_format,Y,M,D,h,m,s);// but I'm sending upper case, so change x to X?	
	} else {
		log_printf(LOG_SCREEN_MIN, "\nExiting time entry.\n");
		Debug ("Exiting time entry...\n");
		slowtalk((unsigned char *) "!!");
		return;
	}
	log_printf(LOG_SCREEN_MAX, "\nsent:  %s\n", out);
	if (high || (!high && ch == 'Y')){
		if (send_line(out,0)){
			if (high) {
				slowtalk((unsigned char *) "!!\r");
				log_printf(LOG_SCREEN_MAX, "!!\n");
				invalidate_eerom_cache();
				invalidate_mt01_cache();
				TalkLstnRestore();
			}
			return; 
		}
	}
	else  //low level and don't synch with clock
		send_line(out,1);// strlen(out) used to be 20

	if (ch == 'Y') {
		nap (1.0); //must be greater than 1 to work at higher bauds.
		Qtalk((unsigned char *) "!S\r");
		//bootstrap to calibrate for device delay and improve synch
		offset = formattime(1);
		Debug("offset = %ld msecs\n", offset);
		//log_printf(LOG_SCREEN_MIN,"offset = %ld msecs\n", offset);
		ftime (&now);
		now.time += 4;
		nap(2.0 - now.millitm/1000.0 + offset/1000.0);
		tm = gmtime (&now.time);
		sprintf(out,time_format, tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		log_printf(LOG_SCREEN_MAX, "\nsent:  %s\n", out);
		if (send_line(out,0)){  //0 flag to send_line requires function to acknowledge old and new eerom received before returning 0 (success)
			if (high) {
				slowtalk((unsigned char *) "!!\r");
				log_printf(LOG_SCREEN_MAX, "!!\n");
				invalidate_eerom_cache();
				invalidate_mt01_cache();
				TalkLstnRestore();
			}
			return; 
		}
		nap (1.0); //must be greater than 1 to work at higher bauds.
		Qtalk((unsigned char *) "!S\r");
	}
	if (high) {
		invalidate_mt01_cache(); //force ^s to be sent	
		TalkLstnRestore();
		log_printf(LOG_SCREEN_MIN, "Clock successfully synchronized, use menu option %c to check time\n", FULL_STATUS);
		//RestoreBaud();
	}
	invalidate_eerom_cache();
}
