#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int ifd;
extern int kfd;

static struct termios cooked_tty;
static struct termios original_tty;

static int networkConnectionFlag = 0;

void
setNetworkConnection (int flag) {
	networkConnectionFlag = flag;
}

int
isNetworkConnection (void) {
	return networkConnectionFlag;
}

// gets current state of serial port (Terminal Control GET ATTRibutes)
// fd is the file descriptor of the serial port that is assigned when opening the serial port
// &saved_tty is an pointer to struct that holds values for the attributes (including baud rate)
void
save_tty (int fd) {
	Debug("save_tty(%d)\n", fd);
	if (tcgetattr (fd, &original_tty) == -1) {
		perror ("tcgetattr");
		exit (1);
	}

	memcpy (&cooked_tty, &original_tty, sizeof (cooked_tty));
#ifdef OXTABS
	cooked_tty.c_oflag |= OXTABS;
#endif
	cooked_tty.c_cc[VEOF] = _POSIX_VDISABLE;
	cooked_tty.c_cc[VINTR] = _POSIX_VDISABLE;
	cooked_tty.c_cc[VQUIT] = _POSIX_VDISABLE;
	cooked_tty.c_cc[VSUSP] = _POSIX_VDISABLE;
	cooked_tty.c_cc[VSTOP] = _POSIX_VDISABLE;
	cooked_tty.c_cc[VSTART] = _POSIX_VDISABLE;

	restore_tty (fd);
}

// TCS definitions specify how anything in the write queue should be dealt with before changing attributes

// restore_tty exits raw mode and puts the tty into the
// mode we want when we're prompting the user for something
void
restore_tty (int fd) {
	Debug("restore_tty on fd %d, kfd is %d\n", fd, kfd);
	if (tcsetattr (fd, TCSAFLUSH, &cooked_tty) == -1) {
		perror ("tcsetattr");
		exit (1);
	}
}

// final_restore_tty exits raw mode and puts the tty back into
// the mode we originally found it in
void
final_restore_tty (int fd) {
	Debug("final_restore_tty(%d)\n", fd);
	if (tcsetattr (fd, TCSAFLUSH, &original_tty) == -1) {
		perror ("tcsetattr");
		exit (1);
	}
}

int
raw (int fd, speed_t speed) {
	struct termios tty;
	int rtn = 0;

	Debug("raw(%d, %ld)\n", fd, cvtBaud (speed));
	if ((fd == ifd) && isNetworkConnection ())
		return rtn;

	if (tcgetattr (fd, &tty) == -1) {
		perror ("tcgetattr");
		restore (1);
	}

	cfmakeraw (&tty); //does most of changing terminal attributes to raw mode.
	if (speed) {
		if (cfsetispeed (&tty, speed)) {
			Debug("cfsetispeed failed: errno=%d (%s)\n", errno, strerror (errno));
			rtn = -1;
		}
		if (cfsetospeed (&tty, speed)) {
			Debug("cfsetospeed failed: errno=%d (%s)\n", errno, strerror (errno));
			rtn = -1;
		}
		// this statement finds which bits are set with CSTOPB and CRTSCTS
		// and unsets them by changing position values to zero and 'AND'ing them with current tty bit values.
		// CSTOPB Set two stop bits, rather than one
		// CRTSCTS (not in POSIX) Enable RTS/CTS (request to send/clear to send hardware) flow control.
		// PARENB parity checking.  We have to turn this off for the program to work on a Mac.
		tty.c_cflag &= ~(CSTOPB | CRTSCTS | PARENB | CSIZE);

		tty.c_cflag |= (CLOCAL | CS8);	// setting CLOCAL bit on to Ignore modem control lines.
	}
	if (tcsetattr (fd, TCSAFLUSH, &tty) == -1) {
		perror ("tcsetattr");
		restore (1);
	}

	return rtn;
}

int
set_speed (int fd, speed_t speed) {
	struct termios tty;

	Debug("set_speed(%d, %ld)\n", fd, cvtBaud (speed));
	if (networkConnectionFlag)
		return 0;

	if (tcgetattr (fd, &tty) == -1) {
		perror ("tcgetattr");
		return 1;
	}

	if (cfsetispeed (&tty, speed)) {
		Debug("cfsetispeed failed: errno=%d (%s)\n", errno, strerror (errno));
		return 1;
	}
	if (cfsetospeed (&tty, speed)) {
		Debug("cfsetospeed failed: errno=%d (%s)\n", errno, strerror (errno));
		return 1;
	}

	if (tcsetattr (fd, TCSAFLUSH, &tty) == -1) {
		perror ("tcsetattr");
		return 1;
	}

	return 0;
}

/* Check to see if the serial device supports 230k baud.
 */
int
is230Valid (void) {
#ifdef B230400
	static int valid_230 = -1;
	struct termios tty;
	struct termios newtty;

	if (networkConnectionFlag)
		return 1;

	if (valid_230 < 0) {
		if (tcgetattr (ifd, &tty) == 0) {
			// Try setting it to 230k
			newtty = tty;
			cfsetispeed (&newtty, B230400);
			cfsetospeed (&newtty, B230400);
			if (tcsetattr (ifd, TCSAFLUSH, &newtty) == 0) {
				if (tcgetattr (ifd, &newtty) == 0) {
					valid_230 = ((cfgetispeed (&newtty) == B230400) &&
								 (cfgetospeed (&newtty) == B230400));
				}
			}
			// Restore baud rate to what is was originally
			tcsetattr (ifd, TCSAFLUSH, &tty);
		}
		return valid_230 < 0 ? 0 : valid_230;
	} else
		return valid_230;
#else
	return 0;
#endif
}

const speed_t *
getValidInstrumentBaudRates (void) {
#ifdef B230400
	static int first = 1;
#endif
	static speed_t validBaud[] = {
		B4800,
		B9600,
		B19200,
/*
#ifdef B28800
		B28800,
#endif
*/
		B38400,
		B57600,
#ifdef B115200
		B115200,
#endif
#ifdef B230400
		B230400,
#endif
		0
	};
	int i;

#ifdef B230400
	if (first) {
		if (!is230Valid ()) {
			for (i = 0; validBaud[i]; i++) {
				if (validBaud[i] == B230400)
					validBaud[i] = 0;
			}
		}
		first = 0;
	}
#endif

	return validBaud;
}

void
opncom (int ifd) {
//	struct termios tty;
	const speed_t *validRates;
	char buf[256];
	unsigned char ch;
	int nbaud;
	int ndx;
	
	// get the Baud rate 

	validRates = getValidInstrumentBaudRates ();
	strcpy (buf, "\n\nBaud rate:");
	for (nbaud = 0; validRates[nbaud]; nbaud++)
		sprintf (&buf[strlen (buf)], "  %d) %ld", nbaud + 1, cvtBaud (validRates[nbaud]));
	strcat (buf, "\n");
	log_printf(LOG_SCREEN_MAX, buf);

	//do {
		readDebug(kfd,&ch,1,"keyboard");
	//	if (ch == ESC)
	//		break;
		log_printf (LOG_SCREEN_MAX, "%c", ch);
	//} while ((ch < '1') || (ch > '0' + nbaud));

	//now open the required port, note baud is long int
	if ((ch >= '1') && (ch <= '0' + nbaud)) {
	//if (ch != ESC) { //else ESC entered, don't change baud.
		ndx = ch - '1';
		if (set_speed (ifd, validRates[ndx]))
			log_printf(LOG_SCREEN_MAX, "\n\n     %ld Baud not supported\n", cvtBaud (validRates[ndx]));
		else
			log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (validRates[ndx]));
		fflush(stdout);
	} else
		log_printf(LOG_SCREEN_MAX, "Baud change canceled. COM is ready at %ld Baud\n", cvtBaud (chkbaud (ifd)));

}

//match_baud matches terminal speed to instrument speed as 
//dictated by line spit from instrument when speed changed 
const char *
match_baud (int ifd, char *sBuf) {
	static const char *baudPat = "speed([[:space:]]+|[[:space:]]+to[[:space:]]+)([[:digit:]]+)";
	regex_t reg;
	regmatch_t pmatch[3];
	char buf[BUFSIZ];
	int rtn;
	char *baud = NULL;
	speed_t speed;

	if ((rtn = regcomp (&reg, baudPat, REG_NEWLINE | REG_EXTENDED | REG_ICASE)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\n", buf);
	} else {
		if ((rtn = regexec (&reg, sBuf, NumElm (pmatch), pmatch, 0)) == 0) {
			baud = &sBuf[pmatch[2].rm_so];
			sBuf[pmatch[2].rm_eo] = '\0';
		}
		regfree (&reg);
	}
	if (baud) {
		speed = cvtSpeed (strtol (baud, NULL, 10));
		if (!speed || set_speed (ifd, speed))
			baud = NULL;	// speed was invalid or set failed
	}
	Debug("match_baud: COM is ready at %ld Baud\n", cvtBaud (speed));
	return baud;
}

speed_t
chkbaud (int fd) {
	struct termios tty;

	if (networkConnectionFlag)
		return 0;

	if (tcgetattr (fd, &tty) == -1) {
		perror ("tcgetattr");
		restore (1);
	}
	return cfgetispeed (&tty);
}


static struct {
	speed_t baud_const;
	unsigned long baud_rate;
} bauds[] = {
    {B0,             0},
    {B50,           50},
    {B75,           75},
    {B110,         110},
    {B134,         134},
    {B150,         150},
    {B200,         200},
    {B300,         300},
    {B600,         600},
    {B1200,       1200},
    {B1800,       1800},
    {B2400,       2400},
    {B4800,       4800},
    {B9600,       9600},
    {B19200,     19200},
#ifdef B28800
    {B28800,     28800},
#endif
    {B38400,     38400},
    {B57600,     57600},
#ifdef B115200
    {B115200,   115200},
#endif
#ifdef B230400
    {B230400,   230400},
#endif
#ifdef B460800
    {B460800,   460800},
#endif
#ifdef B500000
    {B500000,   500000},
#endif
#ifdef B576000
    {B576000,   576000},
#endif
#ifdef B921600
    {B921600,   921600},
#endif
#ifdef B1000000
    {B1000000, 1000000},
#endif
#ifdef B1152000
    {B1152000, 1152000},
#endif
#ifdef B1500000
    {B1500000, 1500000},
#endif
#ifdef B2000000
    {B2000000, 2000000},
#endif
#ifdef B2500000
    {B2500000, 2500000},
#endif
#ifdef B3000000
    {B3000000, 3000000},
#endif
#ifdef B3500000
    {B3500000, 3500000},
#endif
#ifdef B4000000
    {B4000000, 4000000}
#endif
};

long
cvtBaud (speed_t baud) {
	int i;

	for (i = 0; i < NumElm (bauds); i++) {
		if (bauds[i].baud_const == baud)
			return bauds[i].baud_rate;
	}

	return -1;
}

speed_t
cvtSpeed (long speed) {
	int i;

	for (i = 0; i < NumElm (bauds); i++) {
		if (bauds[i].baud_rate == speed)
			return bauds[i].baud_const;
	}

	return 0;
}
