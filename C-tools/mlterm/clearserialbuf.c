#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int ifd;

void clearserialbuf(void) {
	struct timeval tv;
	fd_set rfds;
	int len;
	unsigned char cc[1024];
	
	Debug("Entered clearserialbuf() function.\n");

	for (;;) {
		FD_ZERO (&rfds);
		FD_SET (ifd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 50;  	//this is necessary for clearing out the buffer
							// at the program start up, and also for making sure
							// we don't clear half of a sentence after returning
							// from PAUSE while formating data or time.

		if (select (ifd+1, &rfds, NULL, NULL, &tv) == -1) {
			if (errno != EINTR)
				break;
		} else if (FD_ISSET (ifd, &rfds)) {
			if ((len = readDebug (ifd, cc, sizeof(cc)-1, "instrument")) < 0) {
				perror ("read");
				restore (1);
			} else if (len == 0)	// EOF
				break;
			DebugBuf(len, cc, "trashing %d bytes in serial buffer\n", len);
			cc[len] = '\0';
			log_printf(LOG_SCREEN_MAX, "%s", cc);
		} else
			break;
	}
}
