#include <ctype.h>
#include <fcntl.h> //opening file
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h> // for select
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/param.h>
#include <sys/timeb.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "param.h"
#include "specialkeys.h"
//#include "usermenu.h"

#define SIZE 23	//max input line of '!PPEE0000000000000000\n\0'  
extern int kfd;
extern int ifd;
extern int nfds;


void mltermlin(int pass_thru_only)
{
	fd_set rfds;
	int len;
	int i;
	int store = 0;
	int done = 0;
	unsigned char cc[8192];
	int ofd;
	int sbuflen = 0;
	char sbuf[1024];
	char in[SIZE];
 	int userentry;
	char ch;
 	char buf[BUFSIZ];
	long offset;
	//raw (kfd, 0);	//call this and restore as a wrapper around mltermlin
					// this will cause input entered in the
					// terminal window to be received/processed
					// instantly without having to hit 'ENTER'

	FD_ZERO (&rfds);
	FD_SET (ifd, &rfds);
	FD_SET (kfd, &rfds);
	
	help();
	restore_tty(kfd);
	log_printf(LOG_SCREEN_MIN, "\npress <Enter> to continue..\n");//force user acknowledge response to wake call			
	fgets (buf, sizeof (buf), stdin); 
	log_printf(LOG_SCREEN_MIN, "Terminal ready.\n");
	raw(kfd, 0);
	clearserialbuf();
	Debug ("Entering select loop\n");
	while(done != 1) { 
		FD_ZERO (&rfds);	//if I don't reset all of these, then in the next
		FD_SET (ifd, &rfds); 	//call to select, only whichever fd that was set
		FD_SET (kfd, &rfds);	// to true in   the last call to select
		if (select (nfds, &rfds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR) {
				Debug ("Interrupted system call: no doubt we just nudged the instrument.\n");
			} else {
				Debug ("select returned -1 (errno=%d). Breaking from mltermlin while loop.\n", errno);
				break;
			}
		}
		if (FD_ISSET (ifd,&rfds)){
			if ((len = readDebug (ifd, cc, sizeof(cc), "instrument")) < 0) {
				perror ("read");
				restore (1);
			} 
			cc[len] = '\0';
			log_printf(LOG_SCREEN_MIN, "%s",cc);
			DebugBuf(len, cc, "wrote %d bytes to screen\n", len);
		}//end serial port character detected

		if (FD_ISSET (kfd, &rfds)) {
			//delay here to recieve all the characters that may be part of the hot key (for example, F10 has 5 chars)
			nap (0.10);
			if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
				perror ("read");
				restore (1);
			}
			userentry = isspecialkey(cc, len);
			
			if (cc[0] <= 0x1A && cc[0] != CR) { //ctrl-lower case alphabet
				log_printf(LOG_SCREEN_MIN, "\n^%c", cc[0] - 1 + 'a');
				Debug("user entry: ^%c\n", cc[0] - 1 + 'a');
			}
			
			/*  ALL THINGS THAT SHOULD STILL BE PASSED TO CORK */
			if (userentry == MT01_MENU) {//(cc == 0x13)  // ^s entered
				log_printf(LOG_SCREEN_MIN, "\n");
			}
			else if (userentry == RETURN) {//(cc[0] == CR) 
				if (store) {
					store = 0;
					DebugBuf(sbuflen, (unsigned char *) sbuf, "Complete store command sent that was sent\n");
					sbuflen = 0;
				}
				Debug ("It's a <return>\n");
				log_printf(LOG_SCREEN_MIN, "\n");
			}
			else if ((userentry == STORE) && (!store)) { //((cc[0] == '!') && (!store)) 
				Debug ("It's a !, clear sbuflen and turn on store boolean\n");
				sbuflen = 0;
				store = 1;
			}
			/* THINGS THAT SHOULD NOT BE PASSED TO CORK */
			if (userentry >= 4) {//strchr ("\x1B\x02\x04\x05\x06\x08\x10\x11\x14\x15\x1A\x42\x48", cc[0]))
				if (cc[0] != ESC) Debug ("It's one of the new MLTERM for linux control characters\n");
				else Debug ("It's an <esc> sequence\n");
				if (userentry == BAUD_RATE) {//(cc[0] == STX) ctrl-b baud change requested 
					opncom(ifd);
				} else if (userentry == QUITLOW) { // ESC or ctrl-q
					if (pass_thru_only) {
						restore_tty(kfd);
						if (quit(0)) {
							done = 1;
						}
						else
							raw(kfd,0);
					} else if (quit(0)) {
						if (get_mt01_info(1,1)) { //back in menu, okay to proceed from userfriendly menu
								return;
						} else {
							log_printf(LOG_SCREEN_MIN, "\nInstrument is in an unrecognized state or sleeping.\n");
							log_printf(LOG_SCREEN_MIN, "Did you just deploy the instrument? (y/n) \n");
							readDebug(kfd, &ch, 1, "keyboard");
							log_printf(LOG_SCREEN_MIN, "%c\n", ch);
							Debug("User responded '%c' to 'deploy the instrument?'\n",ch);
							ch = toupper (ch);
							if (ch == 'Y') 
								done = 1;
							else {
								log_printf(LOG_SCREEN_MIN, "Recover MT01 menu and then try again to quit.\n");
							}
						}
					}//end else quit(0)
				} else if (userentry == LINE_EDIT) {
					DebugBuf(sbuflen,(unsigned char *) sbuf,"Entering line edit mode for EEROM entry\n");
					log_printf(LOG_SCREEN_MIN, "\nEntering line edit mode for EEROM entry\n");
					for (i = 0; i < sbuflen; i++) {
						in[i] = sbuf[i];
					}
					writeDebug (kfd, sbuf, sbuflen, "screen"); //don't need to add this to the log 
																// it is only print to screen again to help user remember what has already been entered.
					restore_tty(kfd);
					termln(&in[sbuflen],SIZE-sbuflen); 
					raw(kfd, 0);
					DebugBuf(23,(unsigned char *) &in[sbuflen],"Read EEROM entry from user\n");
					DebugBuf(strlen(in),(unsigned char *) in,"Complete EEROM entry\n");
					if (checkROM(in,sbuflen)) {  //a non-zero return indicates invalid entry.
						if (sbuflen) {
							Debug ("Terminating ! sequence started with newline.\n");
							cc[0] = CR; 
							writeDebug (ifd, cc, 1, "instrument"); //enter in case checkROM canceled user entry after ! already sent
						}
					}
					else {
						if (!send_line(&in[sbuflen], 1)) //successful write to EEROM
							invalidate_eerom_cache();
					}
					store = 0;
					sbuflen = 0;
//				} else if (userentry == OPEN_LOG) {
//					Debug ("log file\n");
//					log_open(NULL);
				} else if (userentry == HELP_MENU) {
					log_printf(LOG_SCREEN_MIN, "\n--accessing MLTERM help --\n");
					Debug("--accessing MLTERM help --\n");
					help();
					printf("\n");
				} else if (userentry == SHOW_BAUD) {
					log_printf(LOG_SCREEN_MIN, "Current terminal baud rate is %d\n", cvtBaud(chkbaud(ifd)) );
				} else if (userentry == FORM_DATALOW) {
					Debug("Turning data formatting on.\nReenter hotkey to cycle through available data formats\n");
						log_printf(LOG_SCREEN_MIN, "Turning data formatting on.\nReenter hotkey to cycle through available data formats\n");
					restore_tty(kfd);
					formatdata(NULL, 0);
					raw(kfd,0);
				} else if (userentry == FORM_TIME) {
					offset = formattime(1);
					log_printf(LOG_SCREEN_MIN, "Instrument clock is %0.3f", abs(offset)/1000.0);
					if (offset < 0)
						log_printf(LOG_SCREEN_MIN, " seconds behind computer clock.\n");
					else
						log_printf(LOG_SCREEN_MIN, " seconds ahead of computer clock.\n");
					log_printf(LOG_SCREEN_MIN, "Continue with formating time strings? (y/n) \n");
					readDebug(kfd, &ch, 1, "keyboard");
					log_printf(LOG_SCREEN_MIN, "%c\n", ch);
					Debug("User responded '%c' to 'continue format timestrings?'\n",ch);
					ch = toupper (ch);
					if (ch == 'Y') {
						Debug ("Turning on time formatting\n");
						log_printf (LOG_SCREEN_MIN, "Turning on time formatting\n");
						formattime(0);
					}
				} else if (userentry == DOWNLOADLOW) {
					Debug ("Download\n");
					restore_tty(kfd);
					ofd = opnfilna();
					raw(kfd,0);
					if (ofd >= 0) 
						download(ofd);
					else {
						log_printf(LOG_SCREEN_MIN, "Failed to open .raw file for download\n");
						cancel(4);
					}
				} else if (userentry == SET_TIMELOW) {
					settime(0);
				} else if (userentry == WAKE) { 
					if (abs(wake(0, 'k')) == 1) //change this back to wake(1), maybe.. just temporarily changed for testing
						log_printf(LOG_SCREEN_MIN, "Failed to wake instrument\n");
				} else if (userentry == PAR_DNLOADLOW) {
					log_printf(LOG_SCREEN_MIN, "Are both Pass Lstn and Pass Talk ON? (y/n) \n");
					readDebug(kfd, &ch, 1, "keyboard");
					log_printf(LOG_SCREEN_MIN, "%c\n", ch);
					Debug("User responded '%c' to 'Pass Lstn and Talk ON?'\n",ch);
					ch = toupper (ch);
					if (ch == 'Y') 
						par_dnload(0,NULL);
					else 
						log_printf(LOG_SCREEN_MIN, "Turn Pass Lstn and Pass Talk ON then try again.\n");
				} else if (userentry == NAKEDROM) {
					log_printf(LOG_SCREEN_MIN, "Are both Pass Lstn and Pass Talk ON? (y/n) \n");
					readDebug(kfd, &ch, 1, "keyboard");
					log_printf(LOG_SCREEN_MIN, "%c\n", ch);
					Debug("User responded '%c' to 'Pass Lstn and Talk ON?'\n",ch);
					ch = toupper (ch);
					if (ch == 'Y') 
						get_meta(0,NULL);
					else 
						log_printf(LOG_SCREEN_MIN, "Turn Pass Lstn and Pass Talk ON then try again.\n");
				} else if (userentry == PAR_UPLOADLOW) {
					log_printf(LOG_SCREEN_MIN, "Are both Pass Lstn and Pass Talk ON? (y/n) \n");
					readDebug(kfd, &ch, 1, "keyboard");
					log_printf(LOG_SCREEN_MIN, "%c\n", ch);
					Debug("User responded '%c' to 'Pass Lstn and Talk ON?'\n",ch);
					ch = toupper (ch);
					if (ch == 'Y') 
						par_upload(0,NULL,NULL);
					else 
						log_printf(LOG_SCREEN_MIN, "Turn Pass Lstn and Pass Talk ON then try again.\n");
				} else if (userentry == PAUSE) {
					log_printf(LOG_SCREEN_MIN, "Pausing print to screen... (hit any key to resume)\n");	
					do {
						FD_ZERO (&rfds);
						FD_SET (kfd, &rfds);
					} while ((select(kfd+1,&rfds, NULL, NULL, NULL) < 0) && (errno == EINTR)); //errno will never = EINTR since alarm isn't set in mltermlin.
					if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
						perror ("read");
						restore(1);
					}
					log_printf(LOG_SCREEN_MIN, "Resuming print to screen.\n");
					clearserialbuf();
				}
			}//end if MLTerm Hotkey 
			else {/* PASS TO CORK */
				if (store) {
					for (i = 0; i < len; i++) {
						sbuf[sbuflen++] = cc[i];
					}
					if (sbuflen >= 4 && !strncmp(sbuf,"!PPE",4)) {
						Debug("WARNING: ! sequence canceled to avoid overwriting PPC EEROM.\n Use ctrl-e to edit EEROM. \n");
						log_printf(LOG_SCREEN_MIN, "\nWARNING: ! sequence canceled to avoid overwriting PPC EEROM.\nUse ctrl-e to edit EEROM. \n");
						cc[0] = CR;
						len = 1;
						store = 0;
						sbuflen = 0 ;
					}
					else if (sbuflen >= 2 && !strncmp(sbuf, "!E", 2)) {
						Debug("WARNING: ! sequence canceled to avoid overwriting RTC EEROM.\n Use ctrl-e to edit EEROM. \n");
						log_printf(LOG_SCREEN_MIN, "\nWARNING: ! sequence canceled to avoid overwriting RTC EEROM.\nUse ctrl-e to edit EEROM. \n");
						cc[0] = CR;
						len = 1;
						store = 0;
						sbuflen = 0;
					}
				} //end if(store)
				writeDebug (ifd, cc, len, "instrument"); 
			} //end else (pass to cork)
		}//end keyboard character detected
	
	}//end while
	log_printf(LOG_SCREEN_MIN, "\nExiting MLterm\n");
	restore (0);
}
