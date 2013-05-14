#include <stdio.h>
#include <fcntl.h>
#include <regex.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>

#include "usermenu.h"
#include "menu.h"
#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int ifd, kfd;

static int expect_baud_change(const char *baud, int force, int speedOK);
static int ackstats(int oldVersion);

int
autodnload (int ofd, unsigned long sectors) {
	Debug ("Automated Download\n");
	const MenuInfo *mi;
	char oldbaud[10], dnldbaud[10];
	int oldVersion;
	int rtn;
	//int ch_speed_indicator;
	int tot_blocks;
	char startblk[10];
	long int firstblock;
	char numsect[10];

	if (!(mi = get_mt01_info(1, 1))){ //forced update because want to see that memory really isn't empty
		Debug("menu parse failed.\n");
		return -1;
	}	
	Debug("Sectors to download %ld out of %ld sectors in memory\n", sectors, mi->mem_used);
	if (mi->mem_used == 0) {
		log_printf(LOG_SCREEN_MIN, "Memory empty.\nEnter sector start (decimal integer starts at 0).\n", MLTERMLIN);
		Debug("Memory empty. Enter sector start.\n");
		Debug("Flush keyboard buffer\n");
		tcflush(kfd, TCIFLUSH);
		if ((firstblock = getint(0,mi->mem_tot))<0) return -4;
		sprintf(startblk,"%08lx", firstblock);
		log_printf(LOG_SCREEN_MIN, "Enter number of sectors to download (decimal integer).\n", MLTERMLIN);
		Debug("Enter number of sectors to download.\n");
		if ((sectors = getint(0,mi->mem_tot-firstblock))<0) return -4;
		sprintf(numsect,"%08lx",sectors); 
	}
	else if (sectors && ( mi->mem_used < sectors)) return -4;
	strcpy(oldbaud,getMenuItem('2',mi)->curr);
	strcpy(dnldbaud,getMenuItem('3',mi)->curr);
	if (strstr(mi->version, "2.1.1") || strstr(mi->version, "2.2.0")) {
		Debug("Detected instrument is old version: %s\n", mi->version);
		oldVersion = 1;
	} else {
		Debug("Detected instrument is new: %s\n", mi->version);
		oldVersion = 0;
	}
	fasttalk((unsigned char *) "D");
	if (strcasecmp(mi->status, "Idle")) {
		if (lstnMT01("Download Data? CAREFUL! (type 'yes')\r\n>")) {
			Debug("Download Data? prompt not received\n");
			return -1;
		}
		else 
			fasttalk((unsigned char *)"yes");
	} else {
		if (lstnMT01("Download Data? (y/n)\r\n>")) {
			Debug("Download Data? prompt not received\n");
			return -1;
		} else
			fasttalk((unsigned char *)"y");
	}
	if (sectors) {
		if (lstnMT01("Get this data? (y/n)\r\n>")) {
			if (strcasestr((char *) get_response_buf(),"CF 00000000 of ")) {
				Debug("memory empty\n");
			}
			else {
				Debug("Neither Get this data? nor CF 00000000 prompt not received\n");
				return -1;
			}
		} else {
			fasttalk((unsigned char *)"n");
			sprintf (startblk, "%08lx", mi->mem_used - sectors);
			sprintf (numsect, "%08lx",sectors);
		}
		fasttalk((unsigned char *) startblk);
		if (oldVersion) fasttalk((unsigned char *) "\x0D");
		fasttalk((unsigned char *) numsect);
		if (oldVersion) fasttalk((unsigned char *) "\x0D");
		if (lstnMT01("Confirm? (y/n)\r\n>")) {
			Debug("not confirmed\n");
			return -1;
		} 
		static const char *lbaPat = "\\(([[:xdigit:]]{8})\\)";
		regmatch_t pmatch[2];
		regex_t reg;
		char buf[BUFSIZ];
		char *cp;


		// Find the number of blocks to be downloaded
		tot_blocks = 0;
		if ((rtn = regcomp (&reg, lbaPat, REG_NEWLINE | REG_EXTENDED | REG_ICASE)) != 0) {
			regerror (rtn, &reg, buf, sizeof (buf));
			fprintf (stderr, "Regex error: %s\r\n", buf);
		} else {
			cp = (char *) get_response_buf ();
			if ((rtn = regexec (&reg, cp, NumElm (pmatch), pmatch, 0)) == 0) {
				tot_blocks = strtol (&cp[pmatch[1].rm_so], NULL, 16);
				Debug ("%d blocks to be downloaded\n", tot_blocks);
			}
			regfree (&reg);
		}
		fasttalk((unsigned char *)"y");	
	} else {
		if (lstnMT01("Get this data? (y/n)\r\n>")) {
			//if get_response_buf("Sectors 00000000 of " Debug("memory empty")....
			Debug("Get this data? prompt not received\n");
			return -1;
		} else {
			static const char *lbaPat = "has ([[:xdigit:]]{8}) LBA";
			regmatch_t pmatch[2];
			regex_t reg;
			char buf[BUFSIZ];
			char *cp;


			// Find the number of blocks to be downloaded
			tot_blocks = 0;
			if ((rtn = regcomp (&reg, lbaPat, REG_NEWLINE | REG_EXTENDED | REG_ICASE)) != 0) {
				regerror (rtn, &reg, buf, sizeof (buf));
				fprintf (stderr, "Regex error: %s\r\n", buf);
			} else {
				cp = (char *) get_response_buf ();
				if ((rtn = regexec (&reg, cp, NumElm (pmatch), pmatch, 0)) == 0) {
					tot_blocks = strtol (&cp[pmatch[1].rm_so], NULL, 16);
					Debug ("%d blocks to be downloaded\n", tot_blocks);
				}
				regfree (&reg);
			}

			fasttalk((unsigned char *)"y");
		}
	}
	if (lstnMT01("Confirmed")) {
		Debug("not confirmed\n");
		return -1;
	}
	if (strcmp(oldbaud, dnldbaud)) {	
		if (expect_baud_change(dnldbaud, 0, 1))
			return -2;
		if (lstnMT01("Awaiting connection.")) {
			Debug("Awaiting connection prompt not received\n");
			if(strcmp(oldbaud, dnldbaud)) {
				if (expect_baud_change(oldbaud, 1, oldVersion)) 
					return -2;
				//add else ackstats  here , nah, we're just lost at this point, accept it.
			}
			return -1;
		}
		//no check on else for Awaiting connection because it comes in after "Confirmed" on "Get this data" prompt when the cntrl speed and download speed are the same
	} 
	Debug("start download\n");
	raw(kfd,0);
	if ((rtn = sbp(ofd, tot_blocks)) != 0) {
		if (rtn == 1) {
			log_printf(LOG_SCREEN_MIN, "\nError reading from terminal window.. exiting.");
			restore(1);
		} else if (rtn == 2) {
			log_printf(LOG_SCREEN_MIN, "\nError reading from DEVTTY.. exiting.");
			restore(1);
		} else if (rtn == 4) { //sbp times out trying to get MT01 to resume download (after 110s)
			//pause until MT01 gives up (120 s) and responds in same way as other exit download sequences.
			Debug("Secure cable connection NOW\n");
			log_printf(LOG_SCREEN_MIN,"Secure cable connection NOW\n");
			nap(12.0);
		}
		restore_tty(kfd); //raw and restore only need to wrap around download sequence (to detect escape character) 
		Debug("Non-normal termination from sector based download, return = %d\n",rtn );
		log_printf(LOG_SCREEN_MIN,"Non-normal termination from sector based download, return = %d\n",rtn );
	} else {
		restore_tty(kfd);
		Debug("download sequence completed\n");
	}	
	if(strcmp(oldbaud, dnldbaud)) {
		if (lstnMT01("speed")) { 
			Debug("Force baud change back to pre-download rate: %s\n", oldbaud);
			if (expect_baud_change(oldbaud, 1, oldVersion))
				return -2; 
		} else {
			Debug("Match change back to pre-download rate: %s\n", oldbaud);
			if (expect_baud_change(oldbaud, 0, oldVersion))
				return -2; 
		}
	}
	//ack stats 
	if (oldVersion || !strcmp(oldbaud,dnldbaud)) {   
		if (lstnMT01("Downloaded")) {
			Debug("No download statistics received\n");
			return -3;
		}
	}
	
	if (ackstats(oldVersion)) {
		Debug("Couldn't parse statistics\n");
		log_printf(LOG_SCREEN_MIN, "Couldn't parse statistics\n");
	}
	if(get_mt01_info(!oldVersion,1) == NULL)  {//if it is oldVersion, we want to use 0 for request_menu flag
		Debug("menu parse failed.\n");
		return -3;
	}
	Debug("autodnload rtn = %d\n", rtn);
	return rtn;
	/*
	ch_speed_indicator = !(lstnMT01("speed"));
	if (!oldVersion || (oldVersion && !strcmp(oldbaud, dnldbaud)))
		ackstats();
	if(strcmp(oldbaud, dnldbaud)) {
		if (ch_speed_indicator) {
			Debug("Force baud change back to pre-download rate: %s\n", oldbaud);
			expect_baud_change(oldbaud, 1, oldVersion); 
		} else {
			Debug("Match change back to pre-download rate: %s\n", oldbaud);
			expect_baud_change(oldbaud, 0, oldVersion);
		}
		if (oldVersion) {
			if (lstnMT01("Acknowledge")) {
				Debug("No download statistics received\n");
				return -1;
			}
			else {
				if (ackstats(1))
					Debug("Couldn't parse statistics\n");
			}
		}
	}
	return 0;
	*/
}
	
static int
expect_baud_change(const char *baud, int force, int speedOK) {
	//for forced and new version of code, don't need speed ok, etc.
	const char *MT01rate;
	int count;
	speed_t speed;
	if (force) {
		MT01rate = baud;
		speed = cvtSpeed (strtol (baud, NULL, 10));
		raw (ifd, speed);
		Debug("COM is ready at %ld Baud\n", cvtBaud (speed));
		log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (speed));
	}
	if (force || (MT01rate = match_baud (ifd, (char *) get_response_buf ())) != NULL) {
		Debug("cntrl speed changed to %s\n",MT01rate);
		if(strcmp(MT01rate, baud)) {
			Debug("MT01 baud rate switch was not as expected (to %s)\n", baud);
			invalidate_mt01_cache ();
		}
	} else {
		Debug("we're lost, call recover function");
		printf("we're lost, call recover function");
		return 1;
	}
	if (!speedOK) return 0;
	else {
		fasttalk((unsigned char *) " ");
		count = 0;
		while(lstnMT01("Speed OK? (y/n)\r\n>") && count < 10) {
			fasttalk((unsigned char *) " ");
			count++;
		}
		if (count == 10) {
			printf("Haven't matched baud, call recover function\n");
			Debug("Haven't matched baud, call recover function\n");
			invalidate_mt01_cache();
			return 1;
		}
		else { 
			fasttalk((unsigned char *) "y");
			invalidate_mt01_cache();
		}
	}
	return 0;
}

static int
ackstats(int oldVersion) {
	static const char *downloadPat = "^(.*download.*)$";
	regmatch_t pmatch[2];
	regex_t reg;
	char buf[BUFSIZ];
	const char *cp;
	int sts;
	int rtn = 1;
	int i;

	Debug("Attempting to ack stats.\n");
	cp = (char *) get_response_buf ();
	log_printf (LOG_SCREEN_MIN, "\n");

	if ((sts = regcomp (&reg, downloadPat, REG_NEWLINE | REG_EXTENDED | REG_ICASE)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\r\n", buf);
	} else {
		for (i = 0; ; i++) {
			if ((sts = regexec (&reg, cp, NumElm (pmatch), pmatch, 0)) != 0)
				break;
			log_printf (LOG_SCREEN_MIN, "%s %.*s\n", (i ? "      " : "Stats:"), (int) (pmatch[1].rm_eo - pmatch[1].rm_so - 1), &cp[pmatch[1].rm_so]);
			cp += pmatch[1].rm_eo;
			rtn = 0;
		}
	}
	if (oldVersion) {
		fasttalk((unsigned char *) "y");
	}
	Debug("ackstats return rtn=%d\n", rtn);
	return rtn;
}
