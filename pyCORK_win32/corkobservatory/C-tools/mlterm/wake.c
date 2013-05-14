#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/timeb.h>
#include <sys/select.h>
#define __USE_GNU
#include <string.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "usermenu.h"
#include "specialkeys.h"

#define USECS 10000 //interval between each character sent
					//this used to be 1000, but sending wake char this rapidly when stuck in sector number
					//entry was causing the instrument to go wiggy. 
					//Tested to see if slower repeat rate fixed the problem by 
					//commenting out countstuck character check.  Yes, slower repeat rate does the trick
					//However, it has caused the wake to not always work within the first 3-4 seconds 
#define INTERVAL 2000 //time to try waking at each baud in millisecs
extern int ifd, kfd;

int wake(int init_baud_only, char wakech){

	struct timeval tv;
	fd_set rfds; 
	unsigned char cc[8192];
	int len;
	int i;
	int pot_woke = 0;
	int countwake = 0;
	int countstuck = 0;
	int sectorstuck = 0;
	const speed_t *validRates;
	struct timeb now;
	struct timeb previous;
	int interval = 0;
	int nbaud;
	int ret;
	speed_t oldBaud;
	tv.tv_sec = 0;
	tv.tv_usec = USECS;
	
	if (!init_baud_only) {
		oldBaud = chkbaud(ifd);
		Debug("oldbaud = %ld\n", cvtBaud(oldBaud));
	}
	log_printf(LOG_SCREEN_MIN, "\nattempting to wake instrument..\n");
	Debug("Trying to wake instrument at %ld baud.\n", cvtBaud(chkbaud(ifd)));
	ftime (&previous);
	slowtalk((unsigned char *) "\x0D");
	while(interval < (INTERVAL + 2000)) { //try harder (1 second longer) at init baud
		FD_ZERO (&rfds);
		FD_SET (ifd, &rfds);
		FD_SET (kfd, &rfds);
		if (writeDebug(ifd,&wakech,1,"instrument") < 0) {
			log_printf(LOG_SCREEN_MIN, "Trouble writing to instrument.\n Attempt to recover by disconnect, pause, and reconnect DB9 \nconnection\n Then choose menu opt %d to recover instrument comms\n", RECOVER); 
			return -1;
		}
		select(ifd+kfd+1, &rfds, NULL, NULL, &tv); // this is not a tight loop. 
		if(FD_ISSET (ifd, &rfds)){
			if ((len = readDebug (ifd, cc, sizeof(cc), "instrument")) < 0) {
				perror ("read");
				restore (1);
			}
			if (len >= 16) {
				cc[len] = '\0';
				
				if (strcasestr((char *) cc, "# of sectors?\r\n>")) {
					log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. Trying to smoothly exit.\n");
					slowtalk((unsigned char *) "\x0D");
					if ((len = readDebug (ifd, cc, sizeof(cc), "instrument")) < 0) {
						perror ("read");
						restore (1);
					}
				}
				if (strcasestr((char *) cc, "Bad value!")) {
					fasttalk((unsigned char *) "\x0Dy");
					if (lstnMT01("Z: Deploy") == 0) {
						log_printf(LOG_SCREEN_MIN, "broke out of sector number prompt during download sequence\n");
						log_printf(LOG_SCREEN_MIN, "Comms successful at %ld baud\n", cvtBaud(chkbaud(ifd)));
						return 0;	
					}
					else {
						log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. Quit wake sequence.");
						return 1;
					}
				}
			}
			for (i = 0; i < len; i++) {
				if (cc[i] == wakech) 
					pot_woke ? countwake++ : (pot_woke = 1); 
				else if (cc[i] == 0x21 || cc[i] == 0x48 || cc[i] ==0x0A || cc[i] == 0x20 || cc[i] == 0xE0)
					countstuck++; 
				else
					pot_woke = countwake = 0;
			}
		}
		else if(FD_ISSET (kfd, &rfds)) {
			if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
				perror ("read");
				restore(1);
			}
			if (isspecialkey(cc,1)==QUITLOW) {
				log_printf(LOG_SCREEN_MIN, "Escaped from wake sequence\n");
				return -2;
			}
		}
	
		if (countstuck > 6) {
			Debug("Instrument stuck at sector number entry. Quit wake sequence at this baud before sending instrument into tissy\n");
			sectorstuck = 1;
			break;	
		}
		if (countwake >= 5) {
			log_printf(LOG_SCREEN_MAX,"Woke instrument at %ld baud.\n", cvtBaud(chkbaud(ifd)));
			Debug("Woke instrument at %ld baud.\n", cvtBaud(chkbaud(ifd)));
			ret = findBaud(init_baud_only);
			if (ret) {
				if (!init_baud_only) {
					set_speed(ifd, oldBaud);
					Debug("COM is ready at %ld Baud\n", cvtBaud(oldBaud));
					log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (oldBaud));
				}
				Debug ("Failed to find instrument menu to begin communications via MLterm.\n");
				log_printf (LOG_SCREEN_MIN, "Failed to find instrument menu to begin communications via MLterm.\n");
			}
			return ret;
		}
		tv.tv_sec = 0;
		tv.tv_usec = USECS;
		ftime (&now);
		interval = 1000 * (now.time - previous.time) + now.millitm - previous.millitm;
	}
	
	if (init_baud_only) {	
		Debug ("Failed to wake instrument\n");
		return -1;
	}
	Debug("Didn't wake instrument at original baud.\n");
	Debug("Try cycling through bauds.\n");
	validRates = getValidInstrumentBaudRates();
	for (nbaud = 0; validRates[nbaud]; nbaud++) {
		
		if (!sectorstuck) Debug("tried for %d milliseconds before switching baud rates\n", interval);
		interval = 0;
		ftime (&previous);
		set_speed(ifd, validRates[nbaud]);
		Debug("COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		slowtalk((unsigned char *) "\x0D");
		while(interval < INTERVAL) {
			FD_ZERO (&rfds);
			FD_SET (ifd, &rfds);
			FD_SET (kfd, &rfds);
			if (writeDebug(ifd,&wakech,1,"instrument") < 0) {
				log_printf(LOG_SCREEN_MIN, "Trouble writing to instrument.\n Attempt to recover by disconnect, pause, and reconnect DB9 \nconnection\n Then choose menu opt %d to recover instrument comms\n", RECOVER); 
				return -1;
			}
			select(ifd+kfd+1, &rfds, NULL, NULL, &tv); // this is not a tight loop. 
			if(FD_ISSET (ifd, &rfds)){
				if ((len = readDebug (ifd, cc, sizeof(cc), "instrument")) < 0) {
					perror ("read");
					restore (1);
				} 
				if (len >= 16) {
					cc[len] = '\0';
					if (strcasestr((char *) cc, "# of sectors?\r\n>")) {
						log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. Trying to smoothly exit\n.");
						slowtalk((unsigned char *) "\x0D");
						if ((len = readDebug (ifd, cc, sizeof(cc), "instrument")) < 0) {
							perror ("read");
							restore (1);
						}
					}
					if (strcasestr((char *) cc, "Bad value!")) {
						fasttalk((unsigned char *) "\x0Dy");
						if (lstnMT01("Z: Deploy") == 0) {
							log_printf(LOG_SCREEN_MIN, "broke out of sector number prompt during download sequence\n");
							log_printf(LOG_SCREEN_MIN, "Comms successful at %ld baud\n", cvtBaud (validRates[nbaud]));
							return 0;	
						}
						else {
							log_printf(LOG_SCREEN_MIN,"Instrument stuck at sector number entry. Quit wake sequence.");
							return 1;
						}
					}
				}
				for (i = 0; i < len; i++) {
					if (cc[i] == wakech) 
						pot_woke ? countwake++ : (pot_woke = 1); 
					else if (cc[i] == 0x21 || cc[i] == 0x48 || cc[i] ==0x0A || cc[i] == 0x20 || cc[i] == 0xE0)
						countstuck++; 
					else 
						pot_woke = countwake = 0;
				}
			}
			else if(FD_ISSET (kfd, &rfds)) {
				if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
					perror ("read");
					restore(1);
				}
				if (isspecialkey(cc,1)==QUITLOW) {
					log_printf(LOG_SCREEN_MIN, "Escaped from wake sequence\n");
					set_speed(ifd, oldBaud);
					Debug("COM is ready at %ld Baud\n", cvtBaud(oldBaud));
					log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (oldBaud));
					return -2;
				}
			}
			if (countstuck > 6) {
				Debug("Instrument stuck at sector number entry. Quit wake sequence at this baud before sending instrument into tissy\n");
				log_printf(LOG_SCREEN_MAX,"Instrument stuck at sector number entry. Quit wake sequence at this baud before sending instrument into tissy\n");
				sectorstuck = 1;
				break;
			}
			if (countwake >= 5) {
				Debug("\nWoke instrument at %ld Baud\n", cvtBaud (validRates[nbaud]));
				log_printf(LOG_SCREEN_MIN, "\nWoke instrument at %ld Baud\n", cvtBaud (validRates[nbaud]));
				ret = findBaud(init_baud_only);
				if (ret) {
					if (!init_baud_only) {
						set_speed(ifd, oldBaud);
						Debug("COM is ready at %ld Baud\n", cvtBaud(oldBaud));
						log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (oldBaud));
					}
					Debug ("Failed to find instrument menu to begin communications via MLterm.\n");
					log_printf (LOG_SCREEN_MIN, "Failed to find instrument menu to begin communications via MLterm.\n");
				}
				return ret;
			}
			tv.tv_sec = 0;
			tv.tv_usec = USECS;
			ftime (&now);
			interval = 1000 * (now.time - previous.time) + now.millitm - previous.millitm;
		}
	}//end for cycle through bauds
	
	set_speed(ifd, oldBaud);
	Debug("COM is ready at %ld Baud\n", cvtBaud(oldBaud));
	log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (oldBaud));
	Debug ("Failed to wake instrument\n");
	return -1;
}

int findBaud(int init_baud_only) {
//findBaud checks the baud rate that we woke the instrument at by 
//requesting a menu at that baud.  Often, the instrument wakes at
//a different baud rate that it is actually communicating at, so 
//this is very necessary!  RETURNS 0 on success, 1 on failure.
	int nbaud;
	const speed_t *validRates;
	char reqmenu = 0x13;
	
	writeDebug(ifd,&reqmenu,1,"instrument");
	if (lstnMT01("Z: Deploy") == 0) {
		log_printf(LOG_SCREEN_MIN, "Comms successful at %ld baud\n", cvtBaud (chkbaud(ifd)));
		return 0;	
	}
	if (init_baud_only) {	
		Debug ("Failed to get instrument menu\n");
		return 1;
	}
	validRates = getValidInstrumentBaudRates();
	
	for (nbaud = 0; validRates[nbaud]; nbaud++) {
		set_speed (ifd, validRates[nbaud]);
		Debug("COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		log_printf(LOG_SCREEN_MAX, "\n\n     COM is ready at %ld Baud\n", cvtBaud (validRates[nbaud]));
		writeDebug(ifd,&reqmenu,1,"instrument");
		if (lstnMT01("Z: Deploy") == 0) {
			log_printf(LOG_SCREEN_MIN, "Comms successful at %ld baud\n", cvtBaud (validRates[nbaud]));
			return 0;	
		}
	}
	
	Debug("findBaud	failed and returning 1\n");
	return 1; 
}


