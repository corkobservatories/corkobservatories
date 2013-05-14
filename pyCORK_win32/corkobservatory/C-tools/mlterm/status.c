#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mltermfuncs.h"
#include "param.h"
#include "usermenu.h"
#include "ttyfuncdecs.h"

#ifndef LONG_LONG_MAX
#	ifdef LLONG_MAX
#		define LONG_LONG_MAX LLONG_MAX
#	else
#		define LONG_LONG_MAX 0x7FFFFFFFFFFFFFFFLL
#	endif
#endif
 
extern int kfd;

int qck_status(void) {
	const MenuInfo *mi;
	int ndx;
	
	if (!(mi = get_mt01_info(1, 1))) { //want to force overwrite of old MT01 params, and at the same time check instrument awake. 
		if (wake(0, 'k')) {
			log_printf(LOG_SCREEN_MIN, "Communication with instrument not established.\n");
			log_printf(LOG_SCREEN_MIN, "Check cable connection.\n");
			log_printf(LOG_SCREEN_MIN, "To retry enter %c for quick status.\n",QCK_STATUS);
			log_printf(LOG_SCREEN_MIN, "If all else fails, enter %c to recover instrument.\n",RECOVER);
			return 1;
		}
		else if (!(mi = get_mt01_info(1, 1))) {
			log_printf(LOG_SCREEN_MIN, "Problem, Try again, or recover\n");
			return 1;
		}
	}
	log_printf(LOG_SCREEN_MIN, "MT01 version %s\n", mi->version);
	log_printf(LOG_SCREEN_MIN, "Instrument is %s\n", mi->status);
	log_printf(LOG_SCREEN_MIN, "%ld of %ld sectors of memory are full\n", mi->mem_used,mi->mem_tot);
	log_printf(LOG_SCREEN_MIN, "MT01 voltage is %ld mV\n", mi->voltage);
	
	for (ndx = 0; ndx < mi->lenMenu; ndx++) {
		if (mi->menuItems[ndx].curr)
			log_printf (LOG_SCREEN_MIN,"%s is %s. Default is %s\n", mi->menuItems[ndx].label, mi->menuItems[ndx].curr, mi->menuItems[ndx].dflt);

	}
	return 0;
}

int
full_status(void) {
//time offset, remaining time left in memory at current sample rate, 
//best and worst case download times, voltage, current, metadata
	const unsigned char *ppc_list;
	const unsigned char *eerom;
	struct param_def *rp, *pp;
	struct tm tm;
	time_t t0;
	time_t t;
	long long offset;
	int i;
	int bps = 8; //time temp and ID
	static char text[40];
	static time_t epoch;
	struct timeb now;
	const MenuInfo *mi;
	unsigned long emptysects;
	unsigned long remainsecs, secs2dnload; 
	int secpsamp, numchannels, temptime, dnldbaud;
	int hh,mm,ss;	

	log_printf (LOG_SCREEN_MIN, "Please be patient...\n");

	/*GET READY TO ACCESS EEROM*/
	TalkLstn(0);
	fasttalk((unsigned char *) "Q");
	Qtalk((unsigned char *) "!Q\r");
	slowtalk((unsigned char *) "!!\r");
	log_printf(LOG_SCREEN_MAX, "!Q\n!!\n");
	
	raw(kfd,0); //raw so that an ESC char is registered (no <enter> required)
	/*GET META DATA*/
	if (!parse_all_eeroms ()) {
		if ((rp = get_rtc_param_def ()) != NULL)
			log_printf (LOG_SCREEN_MIN, "\n\nMetadata:\n");
			log_printf (LOG_SCREEN_MIN, "  RTC:  %s\n", rp[RTC_META_DATA].eeromval);
	} else {
		log_printf(LOG_SCREEN_MIN, "\nCouldn't get necessary EEROM info from instrument.\n");
		Debug("Couldn't get necessary EEROM info from instrument.\n");
		restore_tty(kfd);
		return 1;
	}
	if ((ppc_list = get_ppc_list ()) != NULL) {
		for (i = 0; (i < 8) && ppc_list[i]; i++) {
			if ((pp = get_ppc_param_def (i)) != NULL)
				log_printf (LOG_SCREEN_MIN, "  PPC%d: %s\n", ppc_list[i], pp[PPC_META_DATA].eeromval);
		}
	} else {
		log_printf(LOG_SCREEN_MIN, "\nCouldn't get necessary EEROM info from instrument.\n");
		Debug("Couldn't get necessary EEROM info from instrument.\n");
		restore_tty(kfd);
		return 1;
	}
	restore_tty(kfd);
	
	
	Qtalk((unsigned char *) "!Q\r");
	slowtalk((unsigned char *) "!!\r");
	log_printf(LOG_SCREEN_MAX, "!Q\n!!\n");
	
	/*GET CLOCK OFFSET AND DRIFT RATE*/
	log_printf (LOG_SCREEN_MIN, "Clock:\n");
	offset = formattime(1);
	ftime(&now);
	log_printf(LOG_SCREEN_MIN, "  Current laptop time (UTC):");
	strftime(text, sizeof (text), "%H:%M:%S %d%b%Y", gmtime(&now.time));
	log_printf(LOG_SCREEN_MIN, "%s (CONFIRM THIS TIME IS ACCURATE)\n", text);

	if (offset == LONG_LONG_MAX)
		log_printf(LOG_SCREEN_MIN,"  Problem parsing instrument time\n");
	else {
		//epoch = (offset+now.millitm)/1000 + now.time;
		//log_printf(LOG_SCREEN_MIN, " Instrument time:   ");
		//strftime(text, sizeof (text), "%H:%M:%S %d%b%Y", gmtime(&epoch));
		//log_printf(LOG_SCREEN_MIN, "%s\n", text);
		log_printf(LOG_SCREEN_MIN, "  Instrument clock is %0.3f", abs(offset)/1000.0);
		if (offset < 0)
			log_printf(LOG_SCREEN_MIN, " seconds behind.\n");
		else
			log_printf(LOG_SCREEN_MIN, " seconds ahead.\n");

		if (((eerom = get_rtc_eerom ()) != NULL) &&
			((rp = get_rtc_param_def ()) != NULL)) {
			eerom += rp[START_TIME].offset;
			memset (&tm, sizeof (tm), 0);
			tm.tm_year = (eerom[0] - 19) * 100 + eerom[1];
			tm.tm_mon  = eerom[2] - 1;
			tm.tm_mday = eerom[3];
			tm.tm_hour = eerom[4];
			tm.tm_min  = eerom[5];
			tm.tm_sec  = eerom[6];
			t = timegm (&tm);
			
			time (&t0);
			if (t0 - t < 24 * 60 * 60) 
				log_printf(LOG_SCREEN_MIN,"  Insufficient delta to calculate clock drift.  Try again tomorrow.\n");
			else {
				strftime(text, sizeof (text), "%H:%M:%S %d%b%Y", gmtime(&t));
				log_printf(LOG_SCREEN_MIN,"  Clock drift rate is %0.1f seconds/year\n", (offset/1000.0)/((time(NULL)-t)/(365.25*24*60*60)));
				log_printf(LOG_SCREEN_MIN,"   (calculated over time since clock synchronized : %s)\n",text); 
			}
		} else {
			log_printf(LOG_SCREEN_MIN,"  Unable to determine clock drift rate: problem parsing start time in EEROM\n");
		}
	}

	/*GET TIME UNTIL MEMORY FULL*/
	log_printf (LOG_SCREEN_MIN, "Memory and Sample Rate:\n");
	sscanf(rp[NUM_CHANNELS].eeromval, "%d", &numchannels); 
	if ((ppc_list = get_ppc_list ()) != NULL) {
		for (i = 0; (i < numchannels) && ppc_list[i]; i++) {
			if ((pp = get_ppc_param_def (i)) != NULL) {
				bps += 4;
				Debug ("4 bps added for ppc i = %d\n", i);
				sscanf(pp[TEMP_INTEGRATION_TIME].eeromval, "%d", &temptime); 
				if (temptime != 0) {
					bps += 4;
					Debug ("4 bps for temp of ppc i = %d\n", i);
				}
			}
		}
	}
	bps += 1; //for trailing 00 byte
	Debug ("Number of bytes per sample is %d\n", bps);
	if (!(mi = get_mt01_info(1,1))) //updates the sectors of memory full
		log_printf(LOG_SCREEN_MIN,"  Unable to determine time until memory full: problem parsing MT01 menu\n");
	else {
		log_printf(LOG_SCREEN_MIN, "  Memory is %0.2f%% full\n", mi->mem_used*100.0/mi->mem_tot);
		emptysects = mi->mem_tot - mi->mem_used;
		Debug ("Number of empty sectors is %ld\n", emptysects);
		if (strcasecmp(rp[MODE_SELECTED_PERIOD].eeromval,"1")) {	
			sscanf(rp[BASIC_SAMPLE_PERIOD].eeromval, "%d", &secpsamp);
			Debug ("basic sample period is %d\n", secpsamp);
		} else
			secpsamp = 1;

		ftime(&now);
		if (emptysects >= (long) ((double) (0x7FFFFFFF - now.time) * bps / (506.0 * secpsamp))) {
			log_printf(LOG_SCREEN_MIN, "  At %d second sample rate logger memory will be full after year 2038\n", secpsamp);
		} else {
			remainsecs = (long) (506.0 * emptysects * secpsamp / bps);
			Debug ("Number of remaining seconds until mem full is %ld\n", remainsecs);
			epoch = now.time + remainsecs;
			strftime(text, 19, "%d%b%Y %H:%M:%S", gmtime(&epoch));
			log_printf(LOG_SCREEN_MIN, "  At %d second sample rate logger memory will be full: %s\n", secpsamp, text);
		}
		if (!strcasecmp(rp[MODE_SELECTED_PERIOD].eeromval,"C")) {	
			if (emptysects >= (long) ((double) (0x7FFFFFFF - now.time) * bps / 506.0)) {
				log_printf(LOG_SCREEN_MIN, " Mem. will be full in NEPTUNE mode (1Hz sampling) after year 2038\n");
			} else {
				remainsecs = (long) (506.0 * emptysects / bps);
				epoch = now.time + remainsecs;
				strftime(text, 19, "%d%b%Y %H:%M:%S", gmtime(&epoch));
				//strftime(text, 19, "%H:%M:%S %d%b%Y", gmtime(&epoch));
				log_printf(LOG_SCREEN_MIN, "  Mem. will be full in NEPTUNE mode (1Hz sampling) at: %s\n", text);
			}
		}
	}
	
	/*GIVE ESTIMATE OF TIME TO DOWNLOAD MEMORY*/ 
	log_printf (LOG_SCREEN_MIN, "Download:\n");
	sscanf(getMenuItem(DNLD_BAUD,mi)->curr, "%d", &dnldbaud); 
	secs2dnload = mi->mem_used*512*10*10/(9*dnldbaud);
	hh = secs2dnload / ( 60 * 60);
	mm = (secs2dnload / 60) % 60;
	ss = secs2dnload % 60;
	log_printf(LOG_SCREEN_MIN,"  Download time: %02d:%02d:%02d at %d baud and 90%% efficiency (Moxa device for Linux)\n", hh, mm, ss, dnldbaud);
	secs2dnload = mi->mem_used*512*10*10/(6*dnldbaud);
	hh = secs2dnload / ( 60 * 60);
	mm = (secs2dnload / 60) % 60;
	ss = secs2dnload % 60;
	log_printf(LOG_SCREEN_MIN, "  Download time: %02d:%02d:%02d at %d baud and 60%% efficiency (USBCOMPRO device for Macs)\n", hh, mm, ss, dnldbaud);
	

	
	invalidate_mt01_cache(); // this is only to force menu request on TLRestore
	TalkLstnRestore();

	return 0;
}
