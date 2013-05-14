#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int kfd;
static FILE *pLog;
static int log_level = LOG_SCREEN_MIN;


int
log_open (const char *filename) {
	static char name[16];
	struct tm *tm;
	time_t now;

	if (!filename) {
		time (&now);
		tm = localtime (&now);
		sprintf (name, "%04d%02d%02d.log", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
		filename = name;
	}
	Debug("Opening log file %s\n", filename);

	// Close the log file in the very unlikely event that it's been opened before
	log_close ();

	pLog = fopen(filename,"a");
	if (!pLog) {
		log_printf (LOG_SCREEN_MIN, "\n%s: %s\n", filename, strerror(errno));
		return 1;
	} else {
		time (&now);
		log_printf(LOG_SCREEN_MIN, "Opened log file %s on %s",filename, ctime(&now));	// ctime() includes a '\n'
		return 0;
	}
}

void
log_close (void) {
	if (pLog) {
		fclose (pLog);
		pLog = NULL;
	}
}

int
set_log_level (int level) {
	int old = log_level;

	log_level = level;

	return old;
}

// Prints formatted string to stdout, and to log file if it's open.
// Converts newlines to <cr><lf> for stdout, since it may be in raw
// mode and will need the <cr>s.  Currently also does the same for
// the log file so it will look like a DOS file (bleh).
// Also sanitizes non-printable characters for stdout (but not for
// the log file).
int
log_printf (int level, const char *format, ...) {
	char buf[16384];
	char logbuf[32768];
	char outbuf[32768];
    va_list ap;
	char *nl, *cr;
	char *bp, *lp, *op;
	int errno_sav;
	int rtn = 0;

	// Only print to log file if we print to the screen
	if (log_level < level)
		return 0;

//	if (log_level < LOG_FILE)
//		return 0;

    va_start (ap, format);
	vsnprintf (buf, sizeof (buf), format, ap);
	va_end (ap);

	/*following for loop just inserts a \r in front of the \n if there wasn't already one there.
	It also replaces ctrl and escape characters with ^_ and <esc> */
	nl = cr = NULL;
	for (bp = buf, lp = logbuf, op = outbuf; *bp; bp++) {
		if (*bp == '\n') {
			if (cr != bp - 1)
				*lp++ = *op++ = '\r';
				// To get rid of <cr>s in log file, remove "*lp++ = " above
			*lp++ = *op++ = *bp;
		} else if (*bp == '\r') {
			cr = bp;
			*lp++ = *op++ = *bp;
			// To get rid of <cr>s in log file, remove "*lp++ = " above
		} else if (isprint (*bp) || (*bp == '\t')) {
			*lp++ = *op++ = *bp;
		} else if ((*bp > 0) && (*bp < ESC)) {
			*lp++ = *bp;
			*op++ = '^';
			*op++ = *bp + 'A' - 1;
		} else if (*bp == ESC) {
			*lp++ = *bp;
			*op++ = '<';
			*op++ = 'e';
			*op++ = 's';
			*op++ = 'c';
			*op++ = '>';
		} else {
			*lp++ = *bp;
			*op++ = '.';
		}
	}
	*lp = *op = '\0';

	if (pLog && ((fputs(logbuf, pLog) < 0) || fflush (pLog) < 0)) {
		errno_sav = errno;
		rtn = -1;
	}

	if (level <= log_level) {
		if ((fputs (outbuf, stdout) < 0) || (fflush (stdout) < 0))
			rtn = -1;
		else if (rtn == -1)
			errno = errno_sav;
	}

	return rtn;
}
