#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>

#include "mltermfuncs.h"

// We've got to be a little tricky here in case our alarm goes
// off while we're napping.  In that case we need to figure out
// how much longer we need to sleep and nap some more.
void
nap (float seconds) {
	struct timeval start;
	struct timeval now;
	struct timeval tv;
	long delay_sec;
	long delay_usec;
	int first = 1;

	gettimeofday (&start, NULL);
	delay_sec = (long) seconds;
	delay_usec = (seconds - (long) seconds) * 1000000;

	for (;;) {
		if (first) {
			tv.tv_sec  = delay_sec;
			tv.tv_usec = delay_usec;
			first = 0;
		} else {
			// Figure out how much of our delay remains
			gettimeofday (&now, NULL);
			tv.tv_sec  = delay_sec  - (now.tv_sec  - start.tv_sec);
			tv.tv_usec = delay_usec - (now.tv_usec - start.tv_usec);
			if (tv.tv_usec < 0) {
				tv.tv_usec += 1000000;
				tv.tv_sec--;
			}
			// Make sure we haven't already napped long enough
			if ((tv.tv_sec < 0) || ((tv.tv_sec == 0) && (tv.tv_usec <= 0)))
				break;
			Debug ("Our %ld.%06ld second nap was interrupted: %ld.%06ld seconds remaining to nap.\n",
				   delay_sec, delay_usec, tv.tv_sec, tv.tv_usec);
		}
		if (select (0, NULL, NULL, NULL, &tv) >= 0)
			break;	// No interruption, so we're cool to exit
		else if (errno != EINTR) {
			Debug ("select failed (%d): %s\n", errno, strerror (errno));
			break;
		}
	}
}
