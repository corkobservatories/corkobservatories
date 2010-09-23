// ex: sw=4 ts=4 noexpandtab

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "menu.h"
#include "mltermfuncs.h"
#include "param.h"

#define Free(X)		if (X) free (X)
#define BBUFFSIZE 8192

static int mt01_is_init = 0;
static MenuInfo menuInfo;

static void probe_mt01(int request_menu);
static int parseMenu(const char *menu, MenuInfo *mi);
static void freeMenu(MenuInfo *mi);

#ifdef TEST
#define Debug	printf
#endif

#ifndef TEST

extern int ifd;

const MenuInfo *
get_mt01_info (int request_menu, int force) {
//overwrites cache and leaves program in menu state if request_menu = 0 or force = 1
	if (force || !request_menu)
		invalidate_mt01_cache ();

	if (!mt01_is_init)
		probe_mt01 (request_menu);
	return mt01_is_init ? &menuInfo : NULL;
}

// Call this function if anything has happened that might affect values in the MT01 menu
void
invalidate_mt01_cache () {
	if (mt01_is_init) {
		Debug("invalidate_mt01_cache()\n");
		mt01_is_init = 0;
		freeMenu (&menuInfo);
	}
}


static void
probe_mt01 (int request_menu) {
	int len;
	int totlen;
	fd_set rfds;
	char bBuff[BBUFFSIZE];
	struct timeval tv;
	int i;

	if (mt01_is_init)
		return;

	if (request_menu) {
		clearserialbuf ();
		writeDebug (ifd, "\x13", 1, "instrument");
		log_printf (LOG_SCREEN_MAX, "^s\n");
	}
 
	for (totlen = 0; totlen < BBUFFSIZE - 1; totlen += len) {
		FD_ZERO (&rfds);    //if I don't reset all of these, then in the next
		FD_SET (ifd, &rfds);    //call to select, only whichever fd that was set
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		if (select (ifd+1, &rfds, NULL, NULL, &tv) < 0) { 
			if (errno != EINTR)
				break;
		}
		else if (FD_ISSET (ifd, &rfds)) {
			if ((len = readDebug (ifd, &bBuff[totlen], BBUFFSIZE - totlen - 1, "instrument")) < 0) {
				perror ("read");
				restore (1);
			}
		} else { //timeout after MT01 is  done spitting menu.
			break;
		}
	}

	bBuff[totlen] = '\0';
	log_printf(LOG_SCREEN_MAX,"%s", bBuff);
	
	// Replace any nulls within the buffer
	for (i = 0; i < totlen; i++) {
		if (!bBuff[i])
		bBuff[i] = ' ';
	}
									
	Debug ("Total length of bBuff for Menu is %d\n", totlen);
	DebugBuf (totlen, (unsigned char *) bBuff, "buffer passed to parseMenu\n");
	// If we haven't invalidated and freed before, we'd better do it now
	if (!mt01_is_init)
		freeMenu (&menuInfo);

	if (parseMenu (bBuff, &menuInfo) == 0)
		mt01_is_init = 1;
	else {
		Debug ("Failed to parse MT01 menu\n");
		fprintf (stderr, "MT01 menu parse error! Instrument may be sleeping\r\n");
	}

}

#endif

static int
parseMenu (const char *menu, MenuInfo *mi) {
	static const char *versPat = "MT01 (v[[:digit:].]+[[:space:]]*\\([[:space:]]*[[:digit:]]+[[:space:]]*\\))";
	static const char *voltPat = "Voltage:[[:space:]]*([[:digit:]]+)[[:space:]]*mV";
	static const char *memPat  = "([[:xdigit:]]{8})/([[:xdigit:]]{8})";
	static const char *statPat = "(Idle|Logging)";
	static const char *itemPat = "^([[:alnum:]]):[[:space:]]+((.*[^[:space:]])[[:space:]]+([^[:space:]]+)[[:space:]]+\\([[:space:]]*([^[:space:]]+)[[:space:]]*\\)|([^()]*[^[:space:]]))[[:space:]]*$";
	regex_t reg;
	regmatch_t pmatch[7];
	char buf[BUFSIZ];
	const char *mp;
	int rtn;
	int ndx;

	memset (mi, 0, sizeof (*mi));

	// Parse the version out of the menu
	if ((rtn = regcomp (&reg, versPat, REG_NEWLINE | REG_EXTENDED)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\r\n", buf);
	} else {
		if ((rtn = regexec (&reg, menu, NumElm (pmatch), pmatch, 0)) == 0) {
			mi->version = strndup (&menu[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
			Debug ("Version: \"%s\"\n", mi->version);
		}
		regfree (&reg);
	}
	if (rtn)
		return 1;

	// Parse the voltage out of the menu
	if ((rtn = regcomp (&reg, voltPat, REG_NEWLINE | REG_EXTENDED)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\r\n", buf);
	} else {
		if ((rtn = regexec (&reg, menu, NumElm (pmatch), pmatch, 0)) == 0) {
			mi->voltage = strtol (&menu[pmatch[1].rm_so], NULL, 10);
			Debug ("Voltage: %ld\n", mi->voltage);
		}
		regfree (&reg);
	}
	if (rtn)
		return 1;

	// Find the status: Idle or Logging
	if ((rtn = regcomp (&reg, statPat, REG_NEWLINE | REG_EXTENDED)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\r\n", buf);
	} else {
		if ((rtn = regexec (&reg, menu, NumElm (pmatch), pmatch, 0)) == 0) {
			mi->status = strndup (&menu[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
			Debug ("Status: \"%s\"\n", mi->status);
		}
		regfree (&reg);
	}
	if (rtn)
		return 1;

	// Find the used and total available flash memory
	if ((rtn = regcomp (&reg, memPat, REG_NEWLINE | REG_EXTENDED)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\r\n", buf);
	} else {
		if ((rtn = regexec (&reg, menu, NumElm (pmatch), pmatch, 0)) == 0) {
			mi->mem_used = strtol (&menu[pmatch[1].rm_so], NULL, 16);
			mi->mem_tot  = strtol (&menu[pmatch[2].rm_so], NULL, 16);
			Debug ("Memory: %lu/%lu\n", mi->mem_used, mi->mem_tot);
		}
		regfree (&reg);
	}
	if (rtn)
		return 1;

	// Parse out the various menu options
	if ((rtn = regcomp (&reg, itemPat, REG_NEWLINE | REG_EXTENDED)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\r\n", buf);
	} else {
		for (mp = menu, mi->lenMenu = 0;
			 mi->lenMenu < NumElm (mi->menuItems);
			 mp += pmatch[0].rm_eo, mi->lenMenu++) { //pmatch[0] refers to the entire matched string
			if ((rtn = regexec (&reg, mp, NumElm (pmatch), pmatch, 0)) != 0)
				break;
			mi->menuItems[mi->lenMenu].opt = mp[pmatch[1].rm_so];
			if (pmatch[3].rm_so != -1) {
				mi->menuItems[mi->lenMenu].label = strndup (&mp[pmatch[3].rm_so], pmatch[3].rm_eo - pmatch[3].rm_so);
				mi->menuItems[mi->lenMenu].curr  = strndup (&mp[pmatch[4].rm_so], pmatch[4].rm_eo - pmatch[4].rm_so);
				mi->menuItems[mi->lenMenu].dflt  = strndup (&mp[pmatch[5].rm_so], pmatch[5].rm_eo - pmatch[5].rm_so);
			} else
				mi->menuItems[mi->lenMenu].label = strndup (&mp[pmatch[6].rm_so], pmatch[6].rm_eo - pmatch[6].rm_so);
		}
		regfree (&reg);
	}

	for (ndx = 0; ndx < mi->lenMenu; ndx++) {
		if (mi->menuItems[ndx].curr)
			Debug ("'%c'\t\"%s\"\t\"%s\"\t\"%s\"\n", mi->menuItems[ndx].opt, mi->menuItems[ndx].label, mi->menuItems[ndx].curr, mi->menuItems[ndx].dflt);
		else
			Debug ("'%c'\t\"%s\"\n", mi->menuItems[ndx].opt, mi->menuItems[ndx].label);
	}

	if (mi->lenMenu < 17)
		return 2;

	return 0;
}

// Free memory allocated for MenuInfo struct
static void
freeMenu (MenuInfo *mi) {
	int i;

	Free (mi->version);
	Free (mi->status);

	for (i = 0; i < mi->lenMenu; i++) {
		Free (mi->menuItems[i].label);
		Free (mi->menuItems[i].curr);
		Free (mi->menuItems[i].dflt);
	}

	memset (mi, 0, sizeof (*mi));
}

// Get the MenuItem for a given option character
const MenuItem *
getMenuItem (char opt, const MenuInfo *mi) {
	int i;

	for (i = 0; i < mi->lenMenu; i++) {
		if (mi->menuItems[i].opt == opt)
			return &mi->menuItems[i];
	}

	return NULL;
}

#ifdef TEST
int
main () {
	static const char *menu1 = 
		"junkMT01 v2.2.0 ( 517 ): SanDisk SDCFJ-512        HDX 4.03\r\n"
		"Voltage:  6880 mV: OK\r\n"
		"Logging: 00000A4F/000F45F0\r\n"
		"1: Data  19200   (19200  )\r\n"
		"2: Cntrl 19200   (19200  )\r\n"
		"3: Dwnld 230400  (230400 )\r\n"
		"4: UART  LED ON  (ON )\r\n"
		"5: Power LED ON  (ON )\r\n"
		"6: Error LED ON  (ON )\r\n"
		"7: Write LED ON  (ON )\r\n"
		"8: Pass Lstn ON  (ON )\r\n"
		"9: Pass Talk ON  (OFF)\r\n"
		"0: Lstn Mode BIN  (ASC )\r\n"
		"L: Low Volt  4960 ( 4960)\r\n"
		"O: Ok Volt   6800 ( 6800)\r\n"
		"D: Download Data\r\n"
		"C: Clear Data\r\n"
		"S: Save Config\r\n"
		"R: Reboot\r\n"
		"Q: Quit Menu\r\n"
		"Z: Deploy\r\n"
		"> ";
	static const char *menu2 = 
		"junkMT01 v2.1.1 ( 498 )\r\n"
		"Status: Idle\r\n"
		"Input: OK\r\n"
		"Voltage:  7120 mV\r\n"
		"SanDisk SDCFB-256       \r\n"
		"Rev 3.03\r\n"
		"00000000/0007A2B0\r\n"
		"1: Data  19200   (19200  )\r\n"
		"2: Cntrl 19200   (19200  )\r\n"
		"3: Dwnld 230400  (230400 )\r\n"
		"4: UART  LED OFF (OFF)\r\n"
		"5: Power LED ON  (ON )\r\n"
		"6: Error LED OFF (OFF)\r\n"
		"7: Write LED ON  (OFF)\r\n"
		"8: Pass Lstn ON  (OFF)\r\n"
		"9: Pass Talk ON  (OFF)\r\n"
		"L: Low Volt  4960 ( 4960)\r\n"
		"O: Ok Volt   6800 ( 6800)\r\n"
		"D: Download Data\r\n"
		"C: Clear Data\r\n"
		"S: Save Config\r\n"
		"R: Reboot\r\n"
		"Q: Quit Menu\r\n"
		"Z: Deploy\r\n"
		"> ";
	int rtn;

	rtn = testParse (menu1);
	putchar ('\n');
	rtn += testParse (menu2);

	return 0;
}

int
testParse (const char *menu) {
	MenuInfo mi;
	int rtn;
	int ndx;

	if ((rtn = parseMenu (menu, &mi)) != 0) {
		fprintf (stderr, "parse error! (%d)\r\n", rtn);
	}

	if (mi.version)
		printf ("Version: \"%s\"\n", mi.version);

	if (mi.voltage)
		printf ("Voltage: %d\n", mi.voltage);

	if (mi.status)
		printf ("Status: \"%s\"\n", mi.status);

	printf ("Memory: %lu/%lu\n", mi.mem_used, mi.mem_tot);

	for (ndx = 0; ndx < mi.lenMenu; ndx++) {
		if (mi.menuItems[ndx].curr)
			printf ("'%c'\t\"%s\"\t\"%s\"\t\"%s\"\n", mi.menuItems[ndx].opt, mi.menuItems[ndx].label, mi.menuItems[ndx].curr, mi.menuItems[ndx].dflt);
		else
			printf ("'%c'\t\"%s\"\n", mi.menuItems[ndx].opt, mi.menuItems[ndx].label);
	}
	freeMenu (&mi);

	return rtn;
}
#endif
