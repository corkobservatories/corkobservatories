#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "mltermfuncs.h"
#include "usermenu.h"

extern int ifd;

static void
alarm_handler (int sig) {
	Debug ("Nudging instrument to make sure it doesn't fall asleep...\n");
	log_printf (LOG_SCREEN_MIN, "\nNudging instrument to make sure it doesn't fall asleep...\n");
	//writeDebug (ifd, "\x13", 1, "instrument");  //<cr> will keep the instrument awake, but won't keep inactivity time out
												// and defaults restored from happening (which sends instrument to sleep)
												//this is only tested on v2.1.1 could be different for v2.2.2
	if (get_mt01_info(1,1) == NULL) {
		Debug ("Instrument fell asleep. Use menu option %c to wake.\n",QCK_STATUS);
		log_printf (LOG_SCREEN_MIN, "\nInstrument fell asleep. Use menu option %c to wake.\n", QCK_STATUS);
	}
}

int
initAlarm (void) {
	struct sigaction sa;

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = alarm_handler;
	return sigaction (SIGALRM, &sa, NULL);
}
