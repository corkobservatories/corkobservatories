/* MT01funcs.c */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#define __USE_GNU
#include <string.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "usermenu.h"

extern int ifd,kfd, nfds;

static char get_baud_opt (const char *baud);

int DataBaud(char option, const char *rate) {
	return 0;
}

int CntrlBaud(char option, const char *rate) {
	const MenuItem *item;
	const MenuInfo *mi;
	unsigned char cmd[2];
	char baud_opt;
	const char *MT01rate;
	int count;

	cmd[0] = option;
	cmd[1] = '\0';
	fasttalk(cmd);
	log_printf(LOG_SCREEN_MAX, "\n");
	if(lstnMT01("Set Cntrl Uart to")) {
		Debug("Error, expected MT01 response not received.\n");
		return 1;
	}
	baud_opt = get_baud_opt(rate);
	cmd[0] = baud_opt;
	cmd[1] = '\0';
	fasttalk(cmd);
	log_printf(LOG_SCREEN_MAX, "\n");
	if(lstnMT01("Control speed")) {
		Debug("Warning. Speed not changed, or expected MT01 response not received.\n");
		if (!(mi = get_mt01_info(1, 1))) {
			Debug("menu parse failed.\n");
			if (!(mi = get_mt01_info(1, 1))) {
				Debug("menu parse failed again.\n");
				printf("we're lost, call recover function");	
				Debug("we're lost, call recover function");	
				return -1;
			}
		}
		return 1;
	}
	if ((MT01rate = match_baud (ifd, (char *) get_response_buf ())) != NULL) { 
		if(strcmp(MT01rate, rate)) {
			Debug("Current MT01 value for %c option not as intended\n", option);
			invalidate_mt01_cache ();
		}
		fasttalk((unsigned char *) " ");
		count = 0;
		while(lstnMT01("Speed OK? (y/n)\r\n>")&& count < 10) {
			fasttalk((unsigned char *) " ");
			count++;
		}
		if (count == 10) {
			printf("Haven't matched baud, call recover function\n");
			Debug("Haven't matched baud, call recover function\n");
			return 1;
		}
		else {
			fasttalk((unsigned char *) "y");
			if (!(mi = get_mt01_info(0, 0))) {
				Debug("menu parse failed.\n");
				return 1;
			}
			item = getMenuItem(option, mi);
			if (strcmp(item->curr, rate)) {
				Debug("Current MT01 value for %s not as intended\n", item->label);
				return 1;
			}
			Debug("%s switched to %s\n",item->label, item->curr);	
		} 
	}
	else {
		Debug("we're lost, call recover function");
		printf("we're lost, call recover function");
		return -1;
	}
	return 0;
}


int DwnldBaud(char option, const char *rate) {

	const MenuItem *item;
	const MenuInfo *mi;
	unsigned char cmd[2];
	char baud_opt;

	cmd[0] = option;
	cmd[1] = '\0';
	fasttalk(cmd);
	log_printf(LOG_SCREEN_MAX, "\n");
	if(lstnMT01("Set Download to")) {
		Debug("Error, expected MT01 response not received.\n");
		return 1;
	}
	baud_opt = get_baud_opt(rate);
	cmd[0] = baud_opt;
	cmd[1] = '\0';
	fasttalk(cmd);
	log_printf(LOG_SCREEN_MAX, "\n");
	//fasttalk((unsigned char *) "\r");	
	if (!(mi = get_mt01_info(0, 0))) {
		Debug("menu parse failed.\n");
		invalidate_mt01_cache ();
		fasttalk((unsigned char *) "\x13");
		if (!(mi = get_mt01_info(0, 0))) {
			Debug("menu parse failed again.\n");
			log_printf(LOG_SCREEN_MIN,"we're lost, call recover function");	
			Debug("we're lost, call recover function");	
			return 1;
		}
	}
	item = getMenuItem(option, mi);
	if (strcmp(item->curr, rate)) {
		Debug("Current MT01 value for %c option not as intended\n", option);
		return 1;
	}
	Debug("%s switched to %s\n",item->label, item->curr);	
	return 0;
}

int Pass(char option, const char *toggle) {
	const MenuItem *item;
	const MenuInfo *mi;
	unsigned char cmd[2];

	cmd[0] = option;
	cmd[1] = '\0';
	fasttalk(cmd);
	log_printf(LOG_SCREEN_MAX, "\n");
	if(lstnMT01("? CAREFUL! (type 'yes')\r\n>")) {
		Debug("Error, expected MT01 response not received.\n");
		return 1;
	}
	fasttalk((unsigned char *)"yes");
	log_printf(LOG_SCREEN_MAX, "\n");
	if (!(mi = get_mt01_info(0, 0))) {
		Debug("menu parse failed.\n");
		return 1;
	}
	item = getMenuItem(option, mi);
	if (strcasecmp(item->curr, toggle)) {
		Debug("Current MT01 value for %c option not as intended\n", option);
		return 1;
	}
	Debug("%s switched to %s\n",item->label, item->curr);	
	return 0;
}

int QuickSwitch(char option, const char *toggle) {
	const MenuItem *item;
	const MenuInfo *mi;
	unsigned char cmd[2];

	cmd[0] = option;
	cmd[1] = '\0';
	fasttalk(cmd);
	log_printf(LOG_SCREEN_MAX, "\n");
	if (!(mi = get_mt01_info(0, 0))) {
		Debug("menu parse failed.\n");
		return 1;
	}
	item = getMenuItem(option, mi);
	if (strcasecmp(item->curr, toggle)) {
		Debug("Current MT01 value for %s not as intended\n", item->label);
		return 1;
	}
	Debug("%s switched to %s\n",item->label, item->curr);	
	
	return 0;
}

int LowVolt(char option, const char *mvolt) {
	return 0;
}

int OkVolt(char option, const char *mvolt) {
	return 0;
}


static char *old_baud = NULL;

int
ChangeBaud(const char *rate) {
	const MenuInfo *mi;
	const MenuItem *baud;

	Debug("ChangeBaud\n");
	if (((mi = get_mt01_info (1, 0)) == NULL) ||
		((baud = getMenuItem (OPT_BAUD, mi)) == NULL)) 
		return -1;
	
	Debug("OPT_BAUD='%c'   cntrl baud=0x%X   mi->lenMenu=%d\n", OPT_BAUD, baud, mi->lenMenu);
	if (!old_baud)
		old_baud = strdup (baud->curr);

	Debug ("ChangeBaud: old_baud=%s\n", old_baud);

	if (strcmp(baud->curr, rate)) {
		Debug("Turn %s to %s\n",baud->label, rate);
		if(CntrlBaud(baud->opt,rate)) {
			Debug("Failed to turn %s to %s\n",baud->label, rate);
			return -1;
		}
	}

	return 0;
}

int
RestoreBaud(void){
	const MenuInfo *mi;
	const MenuItem *baud;

	Debug("RestoreBaud\n");
	if (!old_baud)
		return -2;
	if ((mi = get_mt01_info (1, 0)) == NULL)
		return -1;
	
	if ((baud = getMenuItem (OPT_BAUD, mi)) == NULL)
		return -1;


	Debug ("RestoreBaud: old_baud=%s\n", old_baud);

	if (strcmp(baud->curr, old_baud)) {
		Debug("Turn %s to %s\n",baud->label, old_baud);
		if(CntrlBaud(baud->opt,old_baud)) {
			Debug("Failed to turn %s to %s\n",baud->label, old_baud);
			return -1;
		}
	}
	free(old_baud);
	old_baud = NULL;
	return 0;
	
}


int
StatusSwitch(const char *stat_str) {
	const MenuInfo *mi;
	char ctrlR = 0x12;
	char ctrlL = 0x0C;
	char cmd;
	char reqmenu = 0x13;

	fasttalk((unsigned char *)"Q");
	if (strcasecmp(stat_str,"Logging"))
		cmd = ctrlR; //command for stop logging
	else
		cmd = ctrlL; //command for start logging
	writeDebug(ifd, &cmd, 1, "instrument");
	if (lstnMT01("ogging?\r\n (y/n)\r\n>")) {
		Debug("Error: status switch prompt not received\n");
		return 1;
	}
	fasttalk((unsigned char *)"y"); 
	if (lstnMT01("Confirmed\r\n")) {
		Debug("Status change not confirmed\n");
		return 1;
	}
	writeDebug(ifd, &reqmenu, 1, "instrument");
	if (!(mi = get_mt01_info(0, 0))) {
		Debug("menu parse failed.\n");
		return 1;
	}
	if (strcasecmp(mi->status,stat_str)) {
		Debug("Error: Status is still %s\n", mi->status);
		return 1;
	}
	return 0;
}

static char *old_talk = NULL;
static char *old_lstn = NULL;
static char *old_mode = NULL;
static char *old_status = NULL;

int
TalkLstn (int lstn_only) {
	const MenuInfo *mi;
	const MenuItem *talk;
	const MenuItem *lstn;
	const MenuItem *mode;
	
	Debug ("TalkLstn\n");

	if (((mi = get_mt01_info (1, 0)) == NULL) ||
		((talk = getMenuItem (OPT_TALK, mi)) == NULL) ||
		((lstn = getMenuItem (OPT_LSTN, mi)) == NULL))
		return -1;
	
	mode = getMenuItem (OPT_MODE, mi);
	Debug("OPT_MODE='%c'   mode=%s   mi->lenMenu=%d\n", OPT_MODE, mode ? mode->curr : "<null>", mi->lenMenu);

	if (!old_talk)
		old_talk = strdup (talk->curr);
	if (!old_lstn)
		old_lstn = strdup (lstn->curr);
	if (!old_mode && mode)
		old_mode = strdup (mode->curr);
	if (!old_status && !lstn_only)
		old_status = strdup (mi->status);

	Debug ("TalkLstn: old_talk=%s  old_lstn=%s  old_mode=%s  old_status=%s\n", old_talk, old_lstn, old_mode ? old_mode : "", old_status ? old_status : "");

	if (strcasecmp (talk->curr, "ON") && !lstn_only) {
		//first turn logging off if it is on
		if (strcasecmp (mi->status,"Idle")) {
			if (StatusSwitch("Idle"))
				return -1;
		}
		Debug ("Turn %s to ON\n",talk->label);
		if (Pass (talk->opt, "ON"))
			return -1;
	}else if (strcasecmp(talk->curr, "OFF") && lstn_only) {
		Debug ("Turn %s to OFF\n",talk->label);
		if (Pass (talk->opt, "OFF"))
			return -1;
	}

	if (strcasecmp (lstn->curr, "ON")) {
		Debug ("Turn %s to ON\n",lstn->label);
		if (Pass (lstn->opt, "ON"))
			return -1;
	}

	if (mode && !lstn_only && strcasecmp (mode->curr, "BIN")) {
		Debug ("Turn %s to BIN\n",mode->label);
		if (QuickSwitch (mode->opt, "BIN"))
			return -1;
	}
	Debug ("Completed TalkLstn\n");
	return 0;
}

int
TalkLstnRestore () {
	const MenuInfo *mi;
	const MenuItem *talk;
	const MenuItem *lstn;
	const MenuItem *mode;

	Debug ("TalkLstnRestore\n");

	if (!old_talk && !old_lstn && !old_mode && !old_status)
		return -2;

	if ((mi = get_mt01_info (1, 0)) == NULL)
		return -1;

	if (old_talk) {
		if ((talk = getMenuItem (OPT_TALK, mi)) == NULL)
			return -1;
		if (strcasecmp (talk->curr, old_talk)) {
			Debug ("Turn %s to %s\n",talk->label, old_talk);
			if (Pass (talk->opt, old_talk))
				return -1;
		}
		free (old_talk);
		old_talk = NULL;
	}

	if (old_lstn) {
		if ((lstn = getMenuItem (OPT_LSTN, mi)) == NULL)
			return -1;
		if (strcasecmp (lstn->curr, old_lstn)) {
			Debug ("Turn %s to %s\n",lstn->label, old_lstn);
			if (Pass (lstn->opt, old_lstn))
				return -1;
		}
		free (old_lstn);
		old_lstn = NULL;
	}

	if (old_mode) {
		if ((mode = getMenuItem (OPT_MODE, mi)) == NULL)
			return -1;
		if (strcasecmp (mode->curr, old_mode)) {
			Debug ("Turn %s to %s\n",mode->label, old_mode);
			if (QuickSwitch (mode->opt, old_mode))
				return -1;
		}
		free (old_mode);
		old_mode = NULL;
	}
	
	if (old_status) {
		if (strcasecmp(mi->status,old_status)) {
			Debug("Change status to %s\n", old_status);
			if (StatusSwitch(old_status))
				return -1;
		}
		free (old_status);
		old_status = NULL;
	}
	
	Debug ("Completed TalkLstnRestore\n");
	return 0;
}

// Clear MT01 memory 
int
clear_mt01 () {
	const MenuInfo *mi;

	if (confirm("clear the data from memory")){
		fasttalk((unsigned char *) "C");
		if(lstnMT01("Clear data? CAREFUL! (type 'yes')\r\n> ")) {
			Debug("Error, expected MT01 response not received.\n");
			return 1;
		}
		fasttalk((unsigned char *)"yes");
		log_printf(LOG_SCREEN_MAX, "\n");
		if(lstnMT01("Confirmed")) {
			Debug("Error, expected MT01 response not received.\n");
			return 1;
		}
		//clear, like save, requires an extra char after confirming.
		fasttalk((unsigned char *)" ");
		if (!(mi = get_mt01_info(0, 0))) {
			Debug("menu parse failed.\n");
			return 1;
		}
		return 0;
	}
	return 1; 
	
}

int
deploy_mt01() {
	if (confirm("exit MLTERM and say goodbye to instrument")){
		invalidate_mt01_cache(); //this is at the beginning so it happens regardless of how far through this routine we make it
		//writeDebug(kfd, "\x13", 1, "instrument");
		fasttalk((unsigned char *) "Z");
		if(lstnMT01("Loading defaults, logging and sleeping. CAREFUL! (type 'yes')\r\n>")){
			Debug("Error, expected MT01 response not received.\n");
			return 1;
		}
		fasttalk((unsigned char *)"yes");
		log_printf(LOG_SCREEN_MAX, "\n");
		if(lstnMT01("Confirmed")) {
			Debug("Error, expected MT01 response not received.\n");
			return 1;
		}
		return 0;
	}
	return 1;
}
// Saves current MT01 settings as defaults
int
save_mt01 () {
	const MenuInfo *mi;
	
	fasttalk((unsigned char *) "S");
	if(lstnMT01("Save Defaults? CAREFUL! (type 'yes')\r\n>")) {
		Debug("Error, expected MT01 response not received.\n");
		return -1;
	}
	fasttalk((unsigned char *)"yes");
	log_printf(LOG_SCREEN_MAX, "\n");
	if(lstnMT01("Confirmed")) {
		Debug("Error, expected MT01 response not received.\n");
		return -1;
	}else { 
		log_printf(LOG_SCREEN_MAX,"New MT01 defaults saved.\n");
		Debug("New MT01 defaults saved.\n");
	}
	invalidate_mt01_cache();

	if (old_talk) {
		free (old_talk);
		old_talk = NULL;
	}
	if (old_baud) {
		free (old_baud);
		old_baud = NULL;
	}
	
	if (old_mode) {
		free (old_mode);
		old_mode = NULL;
	}
	if (old_lstn) {
		free (old_lstn);
		old_lstn = NULL;
	}
	
	//save, like clear, requires an extra char after confirming.
	fasttalk((unsigned char *)" ");
	if (!(mi = get_mt01_info(0, 0))) {
		Debug("menu parse failed.\n");
		return -1;
	}
	return 0;
}


int recover(void) {
//returns 0 on success, 1  on failure, and -1 for user escaped interupt sequence
//always returns kfd to cooked mode because we only call recover function from 
//high level user friendly mlterm
	const speed_t *validRates;
	int nbaud;
	int rtn;
	speed_t oldBaud;

	if (confirm("interrupt and recover communications")){
		oldBaud = chkbaud(ifd);
		slowtalk((unsigned char *) "\x0B"); //send a cntrl-k
		if (get_mt01_info(1, 1) != NULL)  //although we don't need to request a menu (first flag) if ^k worked, we will,
										 // so if it is an old instrument, there is a chance the menu will come up  
			return 0;
		else {
			raw(kfd,0);
			rtn = wake(1,'k');
			restore_tty(kfd);
			if (rtn == 0) {
				//nap(1.0);
				slowtalk((unsigned char *) "\x0B");
				if (get_mt01_info(1, 1) != NULL)
					return 0;
			} else if (rtn == -2) {
				log_printf(LOG_SCREEN_MIN, "User escaped recover sequence\n");
				return -1;
			}
		}



		validRates = getValidInstrumentBaudRates();

		for (nbaud = 0; validRates[nbaud]; nbaud++) {
			set_speed (ifd, validRates[nbaud]);
			Debug("COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
			log_printf(LOG_SCREEN_MIN, "\n\n     COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
			slowtalk((unsigned char *) "\x0B");
			if (get_mt01_info(1, 1) != NULL)
				return 0;
			else {
				raw(kfd,0);
				rtn = wake(1,'k');
				restore_tty(kfd);
				if (rtn == 0) {
					//nap(1.0);
					slowtalk((unsigned char *) "\x0B");
					if (get_mt01_info(1, 1) != NULL)
						return 0;
				} else if (rtn == -2) {
					log_printf(LOG_SCREEN_MIN, "User escaped recover sequence\n");
					return -1;
				}
			}
		}
	
		set_speed(ifd, oldBaud);
		Debug("COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		log_printf (LOG_SCREEN_MIN, "Failed to recover instrument with kill character (2009 instruments only).\n");
		log_printf (LOG_SCREEN_MIN, "Trying other known character sequences to recover instrument...\n");
		if (recoverOld() > 0) {
			log_printf (LOG_SCREEN_MIN, "Failed to recover instrument. Try again, \nor choose menu option %c to manually recover,\n or wait 40 minutes for instrument defaults to automatically restore themselves.\n", MLTERMLIN);
			return 1;
		}
		else return 0; //recovered 
	} else return -1; //recover not confirmed
}

int recoverOld(void) {
//returns 0 on success
//test!  This likely isn't going to work if we are stuck at sector entry, and recover sequence is called from wrong baud rate to start. Although now that we've added a trigger to exit wake if characteristic '!' are spit back, we may have saved ourselves from this getting stuck
	
	const speed_t *validRates;
	int nbaud;
	int rtn;
	speed_t oldBaud;

	log_printf(LOG_SCREEN_MIN, "Please be patient...\n");
	oldBaud = chkbaud(ifd);
	slowtalk((unsigned char *) "\x0D");
	if (lstnMT01("# of sectors?\r\n>") == 0) {
		log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. Trying to smoothly exit.");
		slowtalk((unsigned char *) "\x0D");
		lstnMT01(NULL);
	}
	if (lstnMT01("Bad value!") == 0) {
		fasttalk((unsigned char *) "\x0Dy");
		if (lstnMT01("Z: Deploy") == 0) {
			return 0;	
		}
		else {
			log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. ");
		}
	}
	raw(kfd,0);
	rtn = wake(1,'y');
	restore_tty(kfd);
	if (rtn == 0 && get_mt01_info(1, 1) != NULL) {
		return 0;
	} else if (rtn == -2) {
		log_printf(LOG_SCREEN_MIN, "User escaped recover sequence\n");
		return -1;
	} else {
		slowtalk((unsigned char *) "\x18");
		raw(kfd,0);
		rtn = wake(1,'y');
		restore_tty(kfd);
		if (rtn == 0 && get_mt01_info(1, 1) != NULL) {
			return 0;
		} else if (rtn == -2) {
			log_printf(LOG_SCREEN_MIN, "User escaped recover sequence\n");
			return -1;
		}
	}


	validRates = getValidInstrumentBaudRates();

	for (nbaud = 0; validRates[nbaud]; nbaud++) {
		set_speed (ifd, validRates[nbaud]);
		Debug("COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		log_printf(LOG_SCREEN_MIN, "\n\n     COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		slowtalk((unsigned char *) "\x0D");
		if (lstnMT01("# of sectors?\r\n>") == 0) {
			log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. Trying to smoothly exit.");
			slowtalk((unsigned char *) "\x0D");
			lstnMT01(NULL);
		}
		if (strcasestr((char *) get_response_buf(),"Bad value!")) {
			fasttalk((unsigned char *) "\x0Dy");
			if (lstnMT01("Z: Deploy") == 0) {
				return 0;	
			}
			else {
				log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry.");
			}
		}
		raw(kfd,0);
		rtn = wake(1,'y');
		restore_tty(kfd);
		if (rtn == 0 && get_mt01_info(1, 1) != NULL) {
			return 0; //recover failed
		} else if (rtn == -2) {
			log_printf(LOG_SCREEN_MIN, "User escaped recover sequence\n");
			return -1;
		} else {
			slowtalk((unsigned char *) "\x18");
			raw(kfd,0);
			rtn = wake(1,'y');
			restore_tty(kfd);
			if (rtn == 0 && get_mt01_info(1, 1) != NULL) {
				return 0;
			} else if (rtn == -2) {
				log_printf(LOG_SCREEN_MIN, "User escaped recover sequence\n");
				return -1;
			}
		}
	}
	set_speed(ifd, oldBaud);
	Debug("COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
	Debug ("Failed to recover instrument\n");
	return 1;
}
	
int 
confirm(char *prompt) {
	char ch[512]; 
	char chUP;
	
	Debug("Flush keyboard buffer\n");
	tcflush(kfd, TCIFLUSH);
	Debug ("%s\n", prompt);
	log_printf(LOG_SCREEN_MIN,"\nAre you sure you want to %s? (y/n) ", prompt); 
	fgets(ch,sizeof(ch),stdin);
	Debug("User responded %c to %s\n",ch[0], prompt);
	chUP = toupper (ch[0]);
	if (chUP == 'Y')
		return 1;
	else
		return 0;
}

static char
get_baud_opt (const char *baud) {
	char baudPat[128];
	char buf[BUFSIZ];
	regmatch_t pmatch[2];
	regex_t reg;
	const char *bp;
	char opt = 'Q';
	int rtn;

	sprintf (baudPat, "^[[:space:]]*([[:digit:]]):[[:space:]]*%s[[:space:]]*$", baud);
	if ((rtn = regcomp (&reg, baudPat, REG_NEWLINE | REG_EXTENDED)) != 0) {
		regerror (rtn, &reg, buf, sizeof (buf));
		fprintf (stderr, "Regex error: %s\n", buf);
	} else {
		bp = (const char *) get_response_buf ();
		if ((rtn = regexec (&reg, (char *) bp, NumElm (pmatch), pmatch, 0)) == 0)
			opt = bp[pmatch[1].rm_so];
		regfree (&reg);
	}
	return opt;
}
