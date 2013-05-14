#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <termios.h>

#include "specialkeys.h"
//#include "usermenu.h"
#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int kfd;

FILE *
opnparamfile(const char *mode, char *name, int namesize){
	char *cp;
	char *ch;
	FILE *file = NULL;
	Debug("Flush keyboard buffer\n");
	tcflush(kfd,TCIFLUSH);
	do {
		Debug("opnfile: mode=\"%s\"\n", mode);
		if (!strcmp(mode,"r")) ch = "input";
		else if (!strcmp(mode,"w")) ch = "output";
		else ch = "???";
		log_printf(LOG_SCREEN_MIN, "\nEnter %s filename: ", ch);
		Debug("Enter %s filename: \n", ch);
		//restore_tty(kfd);	//enter user interface mode
		fgets(name, namesize, stdin);
		//raw(kfd, 0);
		Debug("user entered %s\n", name);
		//log_printf(LOG_SCREEN_MIN, "%s\n", name);
		cp = strchr(name,'\n');
		if (cp) *cp = '\0'; 
		if (name[0] == '\0' || isspecialkey((unsigned char *) name,strlen(name)) == QUITLOW ) {
			log_printf(LOG_SCREEN_MIN, "\nNo %s file opened.\n", ch);
			Debug("No %s file opened.\n", ch);
			return NULL;
		}
		Debug("Opening %s file %s\n", ch, name);
		log_printf(LOG_SCREEN_MIN, "\nOpening %s file %s\n", ch, name);
		if (access(name,F_OK) || !strcmp(mode,"r")) {
			file = fopen(name, mode);
			if (!file) {
				log_printf (LOG_SCREEN_MIN, "\n%s: %s\n", name, strerror(errno));
			}
		}
		else if (!strcmp(mode,"w")) log_printf(LOG_SCREEN_MIN, "\nFile already exists. Try again.\n");
	} while (!file);
	
	return file;
}

