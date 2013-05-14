#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int ifd;
extern int kfd;

void cancel(int n){             // send CANs at 100 msec intervals
	int i;
	
	for(i=0;i<n;i++){
		if (i)
			nap (0.10);
		writeDebug (ifd,"\x18", 1, "instrument");                    // send abort
	}
}



void download(int ofd){
	char ch;
	int rtn;
	int download_mode = 0;

	while (download_mode == 0) {
		log_printf(LOG_SCREEN_MIN, "Select download type:\n");
		log_printf(LOG_SCREEN_MIN, "\n\n     (s) Sector based protocol");
		log_printf(LOG_SCREEN_MIN, "\n     (x) XMODEM-crc");
	//	log_printf(LOG_SCREEN_MIN, "\n     (c) Checksum driven XMODEM");
		log_printf(LOG_SCREEN_MIN, "\n     (d) Dave's streaming protocol");
		log_printf(LOG_SCREEN_MIN, "\n     (ESC) Cancel download");
		log_printf(LOG_SCREEN_MIN, "\n     choice: ");
		readDebug(kfd,&ch,1,"keyboard");
		ch = toupper(ch);
		Debug("user entry: %c\n", ch);
		switch(ch){
		case 'S':               // custom sector based
			log_printf(LOG_SCREEN_MIN, "\nEntering sector based protocol:\n");
			rtn=sbp(ofd, 0);
			download_mode = 1;
			break;
	
		case 'X':               // CRC driven XMODEM
			log_printf(LOG_SCREEN_MIN, "\nEntering XMODEM with CRC protocol:\n");
			download_mode = 1;
			rtn=xmocrc(ofd);
			break;

	/*
		case 'C':               // checksum driven XMODEM
			download_mode = 1;
			rtn=xmochk();
			break;
	*/
		case 'D':               // Dave's screaming download
			log_printf(LOG_SCREEN_MIN, "\nEntering Dave's streaming protocol:\n");
			download_mode = 1;
			rtn=stream(ofd);
			break;
	
		case ESC:
			close(ofd);
			cancel(4);
			return;
		}
	}//end while no selection
	if (rtn == 1) {
		log_printf(LOG_SCREEN_MIN, "\nError reading from terminal window.. exiting.");
		restore(1);
	}
	if (rtn == 2) {
		log_printf(LOG_SCREEN_MIN, "\nError reading from DEVTTY.. exiting.");
		restore(1); 	
	}
	log_printf(LOG_SCREEN_MIN, "\nDownload return code = %d\n",rtn);
	Debug("\nDownload return code = %d\n",rtn);

	return;
}


