#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <termios.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "specialkeys.h"
//#include "usermenu.h"

extern int kfd;

int opnfilna(void){
	char name[MAXPATHLEN];
	int ofd;
	char *cp;
	Debug("Flush keyboard buffer\n");
	tcflush(kfd,TCIFLUSH);
	do {
		log_printf(LOG_SCREEN_MIN, "\nEnter .raw filename:");
		//restore_tty(kfd);//enter user interface mode
		fgets(name, sizeof(name),stdin);
		//raw(kfd, 0);
		cp = strchr(name,'\n');
		if (cp) *cp = '\0';
		if (isspecialkey((unsigned char *) name, strlen(name)) == QUITLOW) name[0] = '\0';
		if (name[0] == '\0') {
			log_printf(LOG_SCREEN_MIN, "\nNo filename entered. Exiting download sequence.\n");
			return -1;
		}
		else {
			if ((cp = strrchr (name, '/')) == NULL)
				cp = name;
			if (strchr (cp, '.') == NULL)      // if no extension entered...
				strcat (name, ".raw");         // add the expected one
		}
		ofd = open(name, O_WRONLY | O_CREAT |O_EXCL, 0644);
		if (ofd < 0) {
			log_printf (LOG_SCREEN_MIN, "\n%s: %s\n", name, strerror(errno));
		}					
	} while (ofd < 0);

	log_printf(LOG_SCREEN_MIN, "\nopened %s\n", name);
	return ofd;
}

