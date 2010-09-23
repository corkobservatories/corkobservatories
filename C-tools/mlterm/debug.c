#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/timeb.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int ifd;

static FILE *dfp = NULL;
static int alarm_timeout = 0;

void
setAlarmTimeout (int timeout) {
	alarm_timeout = timeout;
	alarm (alarm_timeout ? alarm_timeout - 30 : 0);
}

int
initDebug (const char *filename, const char *logfile) {
	char name[MAXPATHLEN];
	struct tm *tm;
	time_t now;
	char *dot;
	char *slash;

	if (!filename) {
		if (logfile) {
			strcpy (name, logfile);
			if ((dot = strrchr (name, '.')) != NULL) {
				slash = strrchr (name, '/');
				if (!slash || dot > slash)
					*dot = '\0';
			}
			strcat (name, ".dbg");
		} else {
			time (&now);
			tm = localtime (&now);
			sprintf (name, "%04d%02d%02d.dbg", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
		}
		filename = name;
	}

	dfp = fopen (filename, "a");
	return dfp != NULL;
}

int
isDebug (void) {
	return dfp != NULL;
}

void
Debug (const char *format, ...) {
	struct timeb tb;
	struct tm *tm;
	va_list ap;

	if (!dfp)
		return;

	ftime (&tb);
	tm = localtime (&tb.time);
	fprintf (dfp, "%02d:%02d:%02d.%03d: ", tm->tm_hour, tm->tm_min, tm->tm_sec, tb.millitm);
	va_start (ap, format);
	vfprintf (dfp, format, ap);
	va_end (ap);
	fflush (dfp);
}

void
DebugBuf (int len, const unsigned char *buf, const char *format, ...) {
	struct timeb tb;
	struct tm *tm;
	va_list ap;
	int i, j;
	int nsp;

	if (!dfp)
		return;

	ftime (&tb);
	tm = localtime (&tb.time);
	fprintf (dfp, "%02d:%02d:%02d.%03d: ", tm->tm_hour, tm->tm_min, tm->tm_sec, tb.millitm);
	va_start (ap, format);
	vfprintf (dfp, format, ap);
	va_end (ap);

	for (i = 0; i < len; i += 16) {
		fprintf (dfp, "  %04X: ", i);
		for (j = i; j < len && j < i + 16; j++) {
			if (j == i + 8)
				fputc (' ', dfp);
			fprintf (dfp, "%02X ", buf[j]);
		}

		nsp = 3 * (i + 16 - j) + ((j < i + 8) ? 1 : 0) + 4;
		fprintf (dfp, "%*s", nsp, "");

		for (j = i; j < len && j < i + 16; j++) {
			if (j == i + 8)
				fputc (' ', dfp);
			fputc (isprint (buf[j]) ? buf[j] : '.', dfp);
		}

		fputc ('\n', dfp);
	}

	fflush (dfp);
}

int
readDebug (int fd, void *buf, size_t count, const char *name) {
	int len;

	len = read (fd, buf, count);

	if (dfp) {
		if (len >= 0)
			DebugBuf (len, (unsigned char *) buf, "Read %d bytes from %s\n", len, name);
		else
			Debug ("Read from %s failed: %s\n", name, strerror (errno));
	}

	// Did someone pull the plug?
	if (len == 0) {
		len = -1;
		errno = ENODEV;
	}

	return len;
}

int
writeDebug (int fd, const void *buf, size_t count, const char *name) {
	int len;

	len = write (fd, buf, count);

	// The instrument goes to sleep after 5 minutes of inactivity,
	// so we'll nudge it after 4.5 minutes to keep it awake
	if (alarm_timeout && (fd == ifd) && (len > 0)) {
		alarm (alarm_timeout - 30); //timeout 30 seconds before instrument itself will timeout
	}

	if (dfp) {
		if (len >= 0)
			DebugBuf (len, (unsigned char *) buf, "Wrote %d of %d bytes to %s\n", len, count, name);
		else
			Debug ("Write to %s failed: %s\n", name, strerror (errno));
	}

	return len;
}

int
writeDebugDelay (int fd, const void *buf, size_t count, const char *name, speed_t baud) {
	int len;
	int rtn;
	speed_t curr;
	float delay = 0.0;

	// Get current baudrate (note that chkbaud will return 0 if it's a network connection)
	curr = chkbaud (fd);

	// Figure out what our intercharacter delay needs to be
	delay = (10.0 / cvtBaud (baud)) - (curr ? 10.0 / cvtBaud (curr) : 0.0);
	if (delay > 0.00001) {
		for (len = 0; len < count; len++) {
			nap (delay);
			if ((rtn = write (fd, &((unsigned char *) buf)[len], 1)) == 0)
				break;
			else if (rtn == -1) {
				len = -1;
				break;
			}
		}
	} else
		len = write (fd, buf, count);

	// The instrument goes to sleep after 30 minutes of inactivity,
	// so we'll nudge it after 29 minutes to keep it awake
	if (alarm_timeout && (fd == ifd) && (len > 0)) {
		alarm (alarm_timeout - 30);
	}

	if (dfp) {
		if (len >= 0) {
			if (delay > 0.00001)
				DebugBuf (len, (unsigned char *) buf, "Wrote %d of %d bytes to %s (simulating %ld baud)\n", len, count, name, cvtBaud (baud));
			else
				DebugBuf (len, (unsigned char *) buf, "Wrote %d of %d bytes to %s\n", len, count, name);
		} else
			Debug ("Write to %s failed: %s\n", name, strerror (errno));
	}

	return len;
}

// The ftime man page says "This function is obsolete.  Don't use it."
// And it doesn't exist on the Mac, so we'll roll our own.
int
ftime (struct timeb *tb) {
	struct timeval tv;

	gettimeofday (&tv, NULL);
	tb->time = tv.tv_sec;
	tb->millitm = tv.tv_usec / 1000;
	tb->timezone = 0;
	tb->dstflag = 0;

	return 0;
}
