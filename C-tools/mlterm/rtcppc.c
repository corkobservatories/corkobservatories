#include <stdio.h>
#include <stdlib.h>

#include "mltermfuncs.h"
#include "param.h"

static int ppc_is_init = 0;
static int rtc_is_init = 0;
static unsigned char ppc_list[8];
static unsigned char ppc_eerom[8][EEROM_BUFSIZ];
static unsigned char rtc_eerom[EEROM_BUFSIZ];


unsigned char *
get_ppc_list () {
	if (!ppc_is_init)
		probe_ppcs ();
	return ppc_is_init ? ppc_list : NULL;
}


unsigned char *
get_ppc_eerom (int ndx) {
	if (!ppc_is_init)
		probe_ppcs ();
	return ppc_is_init ? ppc_eerom[ndx] : NULL;
}

unsigned char *
get_rtc_eerom () {
	if (!rtc_is_init)
		probe_rtc ();
	return rtc_is_init ? rtc_eerom : NULL;
}

// Call this function if the user has changed anything in eerom
void
invalidate_eerom_cache () {
	ppc_is_init = 0;
	rtc_is_init = 0;
}

void
probe_ppcs () {
	int i;
	int ndx = 0;
	int rtn;
	int done = 0;
	static unsigned char ppc_number[8] = {'9','1','2','3','4','5','6','7'};
	static unsigned char ppcmd[5] = {'!','P','9','\r','\0'}; //one extra space for null terminator

	if (!ppc_is_init) {
		// Read ppc data into ppc_eerom and fill ppc_list
		clearserialbuf();
		log_printf(LOG_SCREEN_MIN, "probing PPCs\n");
		Debug("probing PPCs\n");
		for (i=0; i<8; i++)
			ppc_list[i]=0;
        slowtalk((unsigned char *) "Q\r");     		// quit logger menu
		if (!lstnMT01("[2J")) {
			Debug("just quit menu\n");
			nap(1.0);
		}
		//for(i=0; i<8; i++) {
		i = 0;
		while(i < 8 && !done) { 
			Qtalk((unsigned char *) "!Q\r");					// stop RTC
			if (lstnMT01("!Q"))
				Debug("!Q cmd not echoed\n");
			
			ppcmd[2]=ppc_number[i];
			slowtalk(ppcmd);
			if (lstnMT01((char *) ppcmd))
				Debug("%s cmd not echoed\n",ppcmd);
			log_printf(LOG_SCREEN_MIN, "%s\n", (char *) ppcmd);
			slowtalk((unsigned char *) "!PPD\r");       		// dump EEROM
			if ((rtn = lstn(ppc_eerom[ndx])) == 0) {
				ppc_list[ndx++]=ppc_number[i]-'0';
			}
			else { 
				if (rtn == 27) { 
					log_printf(LOG_SCREEN_MIN, "Abort probe_ppcs requested and honored.\n");
					Debug("Abort probe_ppcs requested and honored.\n");
					return;
				}
				else if (rtn == 3) {
					Debug("PPC %c is not electrically connected.\n",ppc_number[i]);
					if (i == 0) {
						log_printf(LOG_SCREEN_MIN, "Error: PPC on RTC board not responding.\n");
						Debug("Error: PPC on RTC board not responding.\n");
						done = 1;
					} else if (ndx == 1 || (ndx > 1 && (ppc_number[i]-'0' == ppc_list[ndx-1]+2))) {
						Debug("No chance for any more PPCs to be electrically connected.\n");
						done = 1;
					} 
				}
				else if (rtn == 4) {
					Debug("Check cable connection.\n");
					log_printf(LOG_SCREEN_MIN, "Check cable connection.\n");
					return;
				}
				else {
					Debug("Read error, exiting...\n");
					log_printf(LOG_SCREEN_MIN, "Read error, exiting...\n");
					restore(2);
				}
			}
			i++;
		}
		if (ppc_list[0]) {
			ppc_is_init = 1;
			DebugBuf(8, ppc_list, "probe_ppcs: ppc_list:\n");
		} else {
			log_printf(LOG_SCREEN_MIN, "ERROR: No PPCs were detected\n");
			Debug("ERROR: No PPCs were detected\n");
		}
	}
}

void
probe_rtc () {
	int rtn;

	if (!rtc_is_init) {
		clearserialbuf();
		log_printf(LOG_SCREEN_MIN, "probing RTC\n");
		Debug("probing RTC\n");
		slowtalk((unsigned char *) "Q\r");     		// quit logger menu
		if (!lstnMT01("[2J")) {
			Debug("just quit menu\n");
			nap(1.0);
		}
		Qtalk((unsigned char *) "!Q\r");   	   	// stop RTC
		if (lstnMT01("!Q\r"))
			Debug("!Q cmd not echoed\n");
		slowtalk((unsigned char *) "!D\r"); 	     	// ask for RTC EEROM
		if ((rtn = lstn(rtc_eerom)) != 0) {
			if (rtn == 27) { 
				log_printf(LOG_SCREEN_MIN, "Abort probe_rtc requested and honored.\n");
				Debug("Abort probe_rtc requested and honored.\n");
			}
			else if (rtn == 3) {
				Debug("No response from RTC.\n");
				log_printf(LOG_SCREEN_MIN, "No response from RTC.\n");
			}
			else if (rtn == 4) {
				Debug("Check cable connection.\n");
				log_printf(LOG_SCREEN_MIN, "Check cable connection.\n");
			}
			else {
				Debug("Read error, exiting ...\n");
				log_printf(LOG_SCREEN_MIN, "Read error, exiting ...\n");
				restore (2);
			}
		}
		else {
			rtc_is_init = 1;
		}
	}
}
