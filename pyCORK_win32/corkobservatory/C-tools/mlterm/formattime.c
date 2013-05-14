#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <sys/timeb.h>

#include "mltermfuncs.h"
#include "specialkeys.h"
#include "usermenu.h"

#ifndef LONG_LONG_MAX
#	ifdef LLONG_MAX
#		define LONG_LONG_MAX LLONG_MAX
#	else
#		define LONG_LONG_MAX 0x7FFFFFFFFFFFFFFFLL
#	endif
#endif

#define INTERVAL 500

static int get_time_str(int detoffset, char *buf);
static long long echo(char *line);

extern int ifd, kfd, nfds;

long long
formattime(int detoffset) {
	char line[64];
	long long offset;
	int count = 0;

	*line = '\0';
	while(1) {
		if (get_time_str(detoffset, line) == 0) {
			if (!detoffset) {
				offset = echo(line);
				Debug("time offset = %ld\n", offset);
			} else { 
				if (count)
					break;
				else
					count++;
			}
		} else
			break;
	}// end while

	if (detoffset) {
		if (*line) {
			offset = echo(line);
			return offset;
		} else {
			Debug("Trouble grabbing time string\n");
			return LONG_LONG_MAX;
		}	
	}
	else
		return 0;
}
	
static int
get_time_str(int detoffset, char *buf){
//should try emptying buf with buf[0] = '\0' so if timeout occurs 
// *line ceases to be true in formattime function.
	int nc = 0;
	int i, len;
	int userentry;
	int interval;
	char line[16384];
	unsigned char cc[8192];
	struct timeb now;
	struct timeb previous;
	fd_set rfds;
	ftime (&previous);

	while (1) {
		FD_ZERO (&rfds);
		FD_SET (ifd, &rfds);
		if (!detoffset)
			FD_SET (kfd, &rfds);
		if (select (nfds, &rfds, NULL, NULL, NULL) == -1) {
			if (errno != EINTR)
				return -1;
		}
		else if (FD_ISSET (ifd, &rfds)) {
			if ((len = readDebug (ifd, cc, sizeof(cc), "instrument")) < 0) {
				perror ("read");
				restore(1);
			}

			ftime (&now);
			interval = 1000 * (now.time - previous.time) + now.millitm - previous.millitm;
			if (cc[0] == '$' && (interval >= INTERVAL || nc >= 32)) {
				Debug("nc = %d\n",nc);
				nc = 0;
			}
			previous = now;
			if (nc + len >= sizeof(line))
				return -1;
			if (nc < 32) {
				for(i=0;i<len;i++){
					line[nc+i] = cc[i];
				}
				nc=nc+len;
				Debug("nc = %d after append\n",nc);
				if (nc >= 32) {
					line[32] = '\0';
					strcpy (buf, line);
					return 0;
				}

			}
		} //end if FD_ISSET (ifd..
		else if (FD_ISSET (kfd, &rfds)) {
			Debug ("keyboard character waiting to be read\n");
			if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
				perror ("read");
				restore(1);
			}
			userentry = isspecialkey(cc, len);
			//if (userentry == OPEN_LOG) {
			//	Debug("log file\n");
			//	log_open(NULL);
			if (userentry == HELP_MENU) { 
				log_printf(LOG_SCREEN_MIN, "\n--accessing MLTERM help --\n");
				Debug("--accessing MLTERM help --\n");
				help();
				printf("\n");
			} else if (userentry == PAUSE) {
				log_printf(LOG_SCREEN_MIN, "Pausing time strings print to screen...(hit any key to resume)\n");
				do {
					FD_ZERO (&rfds); 
					FD_SET (kfd, &rfds);
				} while ((select(kfd+1,&rfds, NULL, NULL, NULL) < 0) && (errno == EINTR));
				if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
					perror ("read");
					restore(1);
				}
				log_printf(LOG_SCREEN_MIN, "Resuming time strings print to screen.\n");
				clearserialbuf();
			} else if (userentry == QUITLOW) {
				log_printf (LOG_SCREEN_MIN, "Exiting format timestring.\n");
				Debug ("Exiting format timestring.\n");
				return -1;
			} else {
				log_printf(LOG_SCREEN_MIN, "Hit <ESC> to quit timestring format\n");
			}
		}
	}//end while
}


static long long
echo(char *line) {
	long Y1,Y2,M,D,h,m,s,c,v;
	char tmp1[4], tmp2[2];
	static time_t epoch;  //why are these static??
	static char text[40];
	int i;
	long long offset;
	struct timeb now;
	ftime(&now);
	Debug("line passed to echo is %s\n",line);
	i = sscanf(line,"%3s%2lx%2lx%2lx%2lx%2lx%2lx%2lx%1s%3lx%3lx%8lx",tmp1,&Y1,&Y2,&M,&D,&h,&m,&s,tmp2,&c,&v,&epoch);
	Debug("scanned variables i = %d\n",i);

	log_printf(LOG_SCREEN_MAX, "%2ld%02ld-%02ld-%02ld %02ld:%02ld:%02ld%5.1fmA%5.1fv   %ld ",Y1,Y2,M,D,h,m,s,(float) c*100/1024, (float) v*13.2/1024,epoch); 
	epoch=epoch+567993600L;
	strftime(text, 19, "%H:%M:%S %d%b%Y", gmtime(&epoch));
	log_printf(LOG_SCREEN_MAX, " --->  %s\n",text);
	offset = (long long) (epoch - now.time) * 1000 - now.millitm;
	return offset;
}
