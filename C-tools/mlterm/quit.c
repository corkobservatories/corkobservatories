#include <ctype.h>
#include "mltermfuncs.h"

extern int kfd;

int quit(int high) {
//returns 1 on YES want to quit
	char ch; 
//	char cmd = 'Q';
	char cc[512];	
	Debug ("Exit?\n");
	if (!high) {
		log_printf(LOG_SCREEN_MIN, "\nAre you sure you want to exit MLterm for advanced users? (y/n)  "); 
		readDebug(kfd, &ch, 1, "keyboard");
		log_printf(LOG_SCREEN_MIN, "%c\n", ch);
	}
	else {
		Debug("Flush keyboard buffer\n");
		tcflush(kfd, TCIFLUSH); //this might screw up scripting or an efficient user
		log_printf(LOG_SCREEN_MIN, "Are you sure you want to exit MLterm? (y/n)  "); 
		fgets(cc,sizeof(cc),stdin);
		ch = cc[0];
	}
	Debug("User responded '%c' to 'exit MLterm?'\n",ch);
	ch = toupper (ch);
	if (ch == 'Y') {
		return 1;
	}
	else{
		log_printf(LOG_SCREEN_MIN, "\n");
		return 0;
	}
}
