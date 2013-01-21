#include <ctype.h>
#include <fcntl.h> //opening file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/timeb.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "usermenu.h"
#include "param.h"

static const char *DEVTTY = "/dev/ttyUSB0";
//#define DEVTTY	"/dev/ttyS1"//this is our Quatech serial port? 
//#define DEVTTY	"/dev/ttyS0"//this is our RS232 serial port? 

#define SECTOR_BUFFER_SIZE	512	

static int getmenuopt (void);

int kfd;
int ifd;
int nfds;

int
main (int argc, char **argv)
{
	static const char *usage =
	//       Try to keep usage within 80 columns
	//       12345678901234567890123456789012345678901234567890123456789012345678901234567890
			"Usage: %s [OPTION] ... [DEVICE]\n"
#ifdef VPN
			"This version of mlterm has relaxed timeout criteria for TCP/IP connections.\n"
#endif
	"  Options:\n"
			"    -b BAUDRATE     baudrate for terminal (should match instrument) deflt: 19200\n"
			"    -C              Clear memory\n"
			"    -d FILENAME     debug output file\n"
			"    -D FILENAME     Download data to raw output file\n"
			"    -p FILENAME     parameter error output file (companion to -W)\n"
			"    -F              Full status report\n"
			"    -G FILENAME     Get eerom and output to parameter file\n"
			"    -h              display this help and exit\n"
			"    -l FILENAME     log output file\n"
			"    -M FILENAME     Metadata (and naked eerom) to  output file\n"
			"    -n HOST[:PORT]  host (and optional network port; default is 4001) for \n"
			"                    networked serial device\n"
			"    -W FILENAME     Write parameter input file to instrument eerom\n"
			"    -I              Initial status report\n"
			"    -P              low level (Pass-thru) mlterm for advanced users\n"
			"    -R              Recover communications\n"
			"    -s SITENAME     name as in sites.txt to format data (companion to -U)\n"
			"    -Y              sYnchronize instrument clock time\n"
			"    -U COUNT        display data in engineering Units for COUNT samples\n"
			"                    enter '' to display til ESC\n"
			"    -v              verbose output to screen and log file\n"
			"  DEVICE is the serial port device file; default is /dev/ttyUSB0\n";
	char buf[BUFSIZ];
	int opt;
 	int usermenuentry;
	int rtn;
	char sbuf[BUFSIZ];
	
	int command_line_func = 0, passthrough = 0,verbose = 0, formdata = 0, download = 0, cleardata = 0;
	int quickstat = 0, fullstat = 0, synchtime = 0, recovercomms = 0;
	
	char *nethost = NULL;
	//char *outparamfilename = NULL;
	FILE *opf = NULL;
	char *inparamfilename = NULL;
	FILE *ipf = NULL;
	char *stringcount = NULL;
	char *debugfilename = NULL;
	char *logfilename = NULL;
	char *errfilename = NULL;
	FILE *ef = NULL;
	char *eeromfilename = NULL;
	char *rawfilename = NULL;
	char *sitename = NULL;
	long speed;
	
	int ofd;
	char *colon;
	char *cp;
	int netport = 4001;
	speed_t baud = B19200;
	int uid;
	int done;
	
	while ((opt = getopt (argc, argv, "b:d:D:e:hCU:G:l:O:M:s:Pvn:QRFT")) != -1) { 
		switch (opt) {
			case 'b':
				speed = strtol (optarg, &cp, 10);
				if (*cp || ((baud = cvtSpeed (speed)) == 0)) {
					fprintf (stderr, "%s: invalid baudrate: \"%s\"\n", argv[0], optarg);
					exit (1);
				}
				break;
			case 'd':
				debugfilename = optarg;
				break;
			case 'D':
				download = 1;
				rawfilename = optarg;
				command_line_func = 1;
				break;
			case 'C':
				cleardata = 1;
				command_line_func = 1;
				break;
			case 'G':
				//outparamfilename = optarg;
				command_line_func = 1;
				ofd = open(optarg, O_WRONLY | O_CREAT |O_EXCL, 0644);
				if (ofd < 0) {
					perror(optarg);
					restore(102);
				}
				close(ofd);
				opf = fopen(optarg, "w");
				if (!opf) {
					perror(optarg);
					restore(102);
				}
				break;
			case 'e':
				//errfilename = optarg;
				ofd = open(optarg, O_WRONLY | O_CREAT |O_EXCL, 0644);
				if (ofd < 0) {
					perror(optarg);
					restore(102);
				}
				close(ofd);
				ef = fopen (optarg, "w");
				if (!ef) {
					perror (optarg);
					restore(102);
				}
				break;
			case 'l':
				logfilename = optarg;
				break;
			case 'O':
				inparamfilename = optarg;
				command_line_func = 1;
				ipf = fopen(optarg, "r");
				if (!ipf) {
					perror(inparamfilename);
					restore(102);
				}
				break;
			case 'M':
				eeromfilename = optarg;
				command_line_func = 1;
				break;
			case 'P':
				passthrough = 1;
				break;
			case 'U':
				formdata = 1;
				command_line_func = 1;
				stringcount = optarg;  //user can enter '' or "" for sitename argument to get infinite data strings
				break;
			case 'Q':
				quickstat = 1;
				command_line_func = 1;
				break;
			case 'R':
				recovercomms = 1;
				command_line_func = 1;
				break;
			case 'F':
				fullstat = 1;
				command_line_func = 1;
				break;
			case 'T':
				synchtime = 1;
				command_line_func = 1;
				break;
			case 'h':
				fprintf (stderr, usage, argv[0]);
				exit (0);
			case 's':
				sitename = optarg;  
				break;
			case 'v':
				verbose = 1;
				set_log_level (LOG_SCREEN_MAX);
				break;
			case 'n':
				nethost = optarg;
				if ((colon = strchr (nethost, ':')) != NULL) {
					cp = colon + 1;
					if (*cp) {
						netport = strtol (cp, &cp, 10);
						if (*cp) {
							fprintf (stderr, "%s: invalid network host: \"%s\"\n", argv[0], nethost);
							exit (1);
						}
					}
					*colon = '\0';
				}
				setNetworkConnection (1);
				break;
			default:
				fprintf (stderr, usage, argv[0]);
				exit (1);
		}
	} //end while(getopt)

	initDebug (debugfilename, logfilename); //forces .dbg file generation if no commandline debugfile argument.
									//uses logfile name as prefix, if no logfile name, uses date.

	if (!nethost && (argc == optind + 1)) {
		DEVTTY = argv[optind];
	} else if (optind != argc) {
		fprintf (stderr, usage, argv[0]);
		exit (1);
	}

	uid = getuid ();
	setuid (geteuid ());  //sets uid to our effective uid (root)

	if (nethost) {
		Debug ("%s started on host \"%s\" port %d\n", argv[0], nethost, netport);
		if ((ifd = netconnect (nethost, netport)) < 0) {
			fprintf (stderr, "%s:%d: %s\n", nethost, netport, strerror (errno));
			exit (1);
		}
	} else {
		Debug ("%s started on tty \"%s\"\n", argv[0], DEVTTY);
		if (!strcmp (DEVTTY, "/dev/ttyUSB0")) {
			sprintf (sbuf, "setserial %s port 2", DEVTTY);
			system (sbuf);
		}

		if ((ifd = open (DEVTTY, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
			perror (DEVTTY);
			exit (1);
		}// DEVTTY is defined above to identify the serial port we're using
	}

	if (flock (ifd, LOCK_EX | LOCK_NB) == -1) {
		Debug ("flock failed: errno=%d (%s)\n", errno, strerror (errno));
		if (errno == EWOULDBLOCK) {
			if (nethost)
				fprintf (stderr, "%s: %s:%d is locked by another user\n", argv[0], nethost, netport);
			else
				fprintf (stderr, "%s: %s is locked by another user\n", argv[0], DEVTTY);
			exit (1);
		}
	}

	if (!nethost) {
		raw (ifd, baud);	// putting serial port attributes in raw mode
							// allows for communications to pass through
							// immediately without any OS introduced delays. //Ali changed
		is230Valid ();		// Check to see if 230k is a valid baud rate
	}
	setuid (uid); //sets uid back to real user ID (not effective user ID root)
				  // files created will then be owned by original user instead of root

	kfd = fileno (stdin);
	save_tty (kfd);	// first save current terminal attributes
					// characteristics so we can restore them
					// back when we need to for gets() on user input
					// and on exiting program.
	nfds = max (ifd, kfd) + 1;
	//FD_ZERO (&rfds);
	//FD_SET (ifd, &rfds);
	//FD_SET (kfd, &rfds);

/**************************START UP SEQUENCE (diff for passthru)*******************************/
	//all exits are now restores to include restoring terminal (kfd) attributes on exit
	if (log_open(logfilename)) //forces log open. if pointer NULL logfile name will be the date
		restore (1);
	
	log_printf(LOG_SCREEN_MIN, "\n\nMLTERM (for 2008 systems with platinum temperature)   %s\n", __DATE__);
	Debug("MLTERM (for 2008 systems with platinum temperature)   %s\n", __DATE__);

	if (!passthrough) {
		clearserialbuf();
		initAlarm ();
		//sequence for dumping parameters to file
		raw(kfd,0);
		if (abs(wake(0, 'k')) == 1) { //wake returns -2 when user esc
			log_printf(LOG_SCREEN_MIN, "\nCommunication with instrument not established.\n");
			log_printf(LOG_SCREEN_MIN, "Check cable connection.\n");
			log_printf(LOG_SCREEN_MIN, "To retry enter %c at menu prompt for quick status.\n",QCK_STATUS);
			log_printf(LOG_SCREEN_MIN, "If all else fails, enter %c to recover instrument.\n",RECOVER);
			if (command_line_func) restore(100);
		}
		restore_tty(kfd);
	}

/*************************COMMAND LINE and PASS THRU***********************************/	
	if (passthrough) {
			set_log_level (LOG_SCREEN_MAX); //otherwise, default is LOG_SCREEN_MIN (see log_level decl. in log_printf.c)
			raw(kfd,0);
			mltermlin(1); //flag 1 means passthrough only
	}
	if (command_line_func) {
		if (verbose)
			set_log_level (LOG_SCREEN_MAX);
		if (ipf) {
			// If we don't have an error file, but we do have an input file,
			// try to figure out an error file name from the input file name
			if (!ef) { 
				char filename[MAXPATHLEN];
				char *dot;
				char *slash;

				strcpy (filename, inparamfilename);
				if ((dot = strrchr (filename, '.')) != NULL) {
					slash = strrchr (filename, '/');
					if (!slash || dot > slash)
						*dot = '\0';
				}
				strcat (filename, ".perr");
				errfilename = strdup (filename);
				ofd = open(errfilename, O_WRONLY | O_CREAT |O_EXCL, 0644);
				if (ofd < 0) {
					perror(errfilename);
					restore(102);
				}
				close(ofd);
				ef = fopen (errfilename, "w");
				if (!ef) {
					perror (errfilename);
				}
			}//end use inparamfilename	
			if (par_upload(1,ipf,ef) == 1) {
				log_printf(LOG_SCREEN_MIN, "\nUser cancelled or ERROR. Try again\n");
				restore(101);
			}
		} 
		
		if (opf) {
			if (par_dnload(1,opf) == 1)
				log_printf(LOG_SCREEN_MIN, "\nUser cancelled or ERROR. Try again\n");
		}
		if (download) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
			/*
			ofd = open(rawfilename, O_WRONLY | O_CREAT |O_EXCL, 0644);
			if (ofd < 0) {
				perror(rawfilename);
				restore(102);
			}
			close(ofd);
			if (ofd < 0) {
				continue;
			} else {
				setAlarmTimeout(0); // download sequence uses read not readDebug, so instrument will get sent a nudge char in middle of download if we don't clear alarm timeout time
				rtn = autodnload(ofd);
				if (rtn == -1)
					log_printf(LOG_SCREEN_MIN, "Download sequence failed to initiate. Check connection.\n");
				else if (rtn == -2) 
					log_printf(LOG_SCREEN_MIN, "Baud rate problems. Check connection. May have to force recover.\n");
				else if (rtn == -3)
					log_printf(LOG_SCREEN_MIN, "Download stats not received. Check connection.\n");
				else {
					get_mt01_info(1, 1);
				}
			}
			*/
		}
		if (cleardata) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
		}
		if (formdata) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
			if (!stringcount){
			}
			if (sitename) {
			}
		}
		if (quickstat) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
		}
		if (fullstat) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
		}
		if (synchtime) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
		}
		if (recovercomms) {
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
		}
		if (eeromfilename) {	
			log_printf(LOG_SCREEN_MIN,"This command line function hasn't yet been implemented\n");
		}
	}
			
/*************************USER FRIENDLY INTERFACE**********************************/			
//consider changing this if check to if not quit or if not deploy

	if (!command_line_func &&  !passthrough) {
		log_printf(LOG_SCREEN_MIN, "\npress <Enter> to continue..\n");//force user acknowledge response to wake call			
		fgets (buf, sizeof (buf), stdin); 
		//instrument will time out in 5 minutes (if not in menu, like during formatdata), nudge before then
		usermenu();
		done = 0;
		while(!done){
			setAlarmTimeout (1800); 
			log_printf(LOG_SCREEN_MIN, "\nEnter command (H for menu help) >");
			Debug("Flush keyboard buffer\n");
			tcflush(kfd, TCIFLUSH);
			usermenuentry = getmenuopt();
			if (usermenuentry == QUIT) { // ESC or ctrl-q
				if (quit(1)) done = 1;
			} else if (usermenuentry == HELP) {
				usermenu();
			} else if (usermenuentry == QCK_STATUS) {
				log_printf(LOG_SCREEN_MIN, "\n");
				qck_status();
			} else if (usermenuentry == FULL_STATUS) {
				log_printf(LOG_SCREEN_MIN, "\n");
				full_status();
				log_printf(LOG_SCREEN_MIN, "\n");
			} else if (usermenuentry == DEPLOY) {
				if (deploy_mt01())
					log_printf(LOG_SCREEN_MIN, "\nInstrument deployment not completed\n");	
				else {
					done = 1;
					log_printf(LOG_SCREEN_MIN, "\nInstrument successfully deployed\n");
				}
			} else if (usermenuentry == FORM_DATA) {
				setAlarmTimeout (300);  //problem with this is that if 2 samples don't occur with this 4min30 second,  
										//1Hz will never be triggered even if high voltage is applied 
										//(entering and quitting menu causes restart at autonomous mode, and >=2 high volt
										//samples required thereafter to trigger switch)
				formatdata(sitename, 1);
			} else if (usermenuentry == DOWNLOAD) {
				ofd = opnfilna();
				if (ofd < 0) {
					continue;
				} else {
/*  
//This following code was added just for demonstration purposes for the NEPTUNE automated download sequence
					char ch[512];
					char chUP;
					int bytesPsamp, samples;
					tcflush(kfd, TCIFLUSH);
					Debug ("%s\n");
					log_printf(LOG_SCREEN_MIN,"\nDo you want to download all of the data? (y/n) "); 
					fgets(ch,sizeof(ch),stdin);
					Debug("User responded %c\n",ch[0]);
					chUP = toupper (ch[0]);
					if (chUP == 'Y') {
						setAlarmTimeout(0); // download sequence uses read not readDebug, so instrument will get sent a nudge char in middle of download if we don't clear alarm timeout time
						rtn = autodnload(ofd, 0); //will also return -4 if mem empty, or 27, 28 if abort, but these cases statements have already been printed to screen
					} else if (chUP == 'N') {
						log_printf(LOG_SCREEN_MIN,"\nEnter number of most recent samples to download: "); 
						if ((samples = getint(0,INT_MAX))<0)

static int getmenuopt (void);
							rtn = -1;
						else {
							log_printf(LOG_SCREEN_MIN,"\nEnter number of bytes per sample (bytes_per_sample mod 4 should = 1): "); 
							//except for 2007 CORKS which, when plugged into NEPTUNE running at 1Hz, have no 00 byte at the end.
							if ((bytesPsamp = getint(0,65))<0)
								rtn = -1;
							else {
								//pass number of most recent sectors to autodnload function
								//data storage per sector =506
								setAlarmTimeout(0); // download sequence uses read not readDebug, so instrument will get sent a nudge char in middle of download if we don't clear alarm timeout time
								rtn = autodnload(ofd, (samples*bytesPsamp+505)/506);
							}
						}
					}
					else rtn = -1;
*/
					setAlarmTimeout(0); // download sequence uses read not readDebug, so instrument will get sent a nudge char in middle of download if we don't clear alarm timeout time
					rtn = autodnload(ofd, 0); //will also return -4 if mem empty, or 27, 28 if abort, but these cases statements have already been printed to screen
					if (rtn == -1)
						log_printf(LOG_SCREEN_MIN, "Download sequence failed to initiate. Check connection.\n");
					else if (rtn == -2) 
						log_printf(LOG_SCREEN_MIN, "Baud rate problems. Check connection. May have to force recover.\n");
					else if (rtn == -3)
						log_printf(LOG_SCREEN_MIN, "Download stats not received. Check connection.\n");
					else {
						get_mt01_info(1, 1);
					}
				}
			} else if (usermenuentry == MEM_CLEAR) {
				if (clear_mt01())
					log_printf(LOG_SCREEN_MIN, "\nData not cleared from memory\n");
				else
					log_printf(LOG_SCREEN_MIN, "\nMemory erased\n");
			} else if (usermenuentry == SET_TIME) {
				settime(1);
			} else if (usermenuentry == PAR_UPLOAD) {
				log_printf(LOG_SCREEN_MIN, "\nPlease be patient...\n");
				if (par_upload(1,ipf,ef) == 1)
					log_printf(LOG_SCREEN_MIN, "\nUser cancelled or ERROR. Try again\n");
			} else if (usermenuentry == PAR_DNLOAD) {
				log_printf(LOG_SCREEN_MIN, "\nPlease be patient...\n");
				if (par_dnload(1,NULL) == 1)
					log_printf(LOG_SCREEN_MIN, "\nUser cancelled or ERROR. Try again\n");
			} else if (usermenuentry == GET_META) {
				log_printf(LOG_SCREEN_MIN, "\nPlease be patient...\n");
				if (get_meta(1,NULL))
					log_printf(LOG_SCREEN_MIN, "\nUser cancelled or ERROR. Try again\n");
			} else if (usermenuentry == MLTERMLIN) {
				writeDebug(ifd, (unsigned char *) "Q", 1, "instrument");
				//fasttalk((unsigned char *) "Q");
				clearserialbuf();
				set_log_level (LOG_SCREEN_MAX);
				raw(kfd,0);
				setAlarmTimeout(0);
				mltermlin(0);
				restore_tty(kfd);
				if (!verbose) set_log_level (LOG_SCREEN_MIN);
				log_printf(LOG_SCREEN_MIN, "\n\n\n\n\n\n");
				usermenu();
			} else if (usermenuentry == RECOVER) {
				if (recover())
					log_printf(LOG_SCREEN_MIN, "\nRecover not confirmed\n");
				else 
					log_printf(LOG_SCREEN_MIN, "\nInstrument communications restored\n");
			} else if (usermenuentry > -2) {
				log_printf(LOG_SCREEN_MIN, "\nUnrecognized command.\n");	
				usermenu();
			} else {
				log_printf(LOG_SCREEN_MIN, "\nTrouble reading from keyboard. Exiting..\n");	
				restore(3);
			}//end else 
		}//end while 
	
	}//end if 
	log_printf(LOG_SCREEN_MIN, "\nClosing log file at program exit\n");
	log_close ();
	Debug("Closed log file.\n");
	restore (0);
	return 0; //this is just here to avoid compile warning, it is redundant though.
}


int
getint (int min, int max) {
	char buf[BUFSIZ];
	char *ep;
	int rtn;

	while (fgets (buf, sizeof (buf), stdin) == NULL) {
		if (errno != EINTR)
			return -2;
	}
	trim_user_ent (buf);
	Debug("user entry: %s\n",buf);
	log_printf(LOG_SCREEN_MIN,"%s\n", buf);

	if (*buf) {
		rtn = (int) strtol (buf, &ep, 10);
		if (!*ep && (rtn >= min) && (rtn <= max))
			return rtn;
		log_printf (LOG_SCREEN_MIN, "\nInvalid entry (%s).", buf);
	}
	return -1;
}

static int
getmenuopt () {
	char buf[BUFSIZ];

	while (fgets (buf, sizeof (buf), stdin) == NULL) {
		if (errno != EINTR)
			return -2;
	}

	trim_user_ent (buf);
	Debug ("menu opt: %s\n",buf);
	log_printf(LOG_SCREEN_MIN,"%s\n", buf);

	if (!buf[1]) {
		switch (*buf) {
			case HELP:
			case QCK_STATUS:
			case FULL_STATUS:
			case GET_META:
			case DOWNLOAD:
			case MEM_CLEAR:
			case SET_TIME:
			case FORM_DATA:
			case PAR_DNLOAD:
			case PAR_UPLOAD:
			case DEPLOY:
			case QUIT:
			case MLTERMLIN:
			case RECOVER:
				return *buf;
		}
	}


	if (*buf)
		log_printf (LOG_SCREEN_MIN, "\nInvalid entry (%s).", buf);
	return -1;
}
	

// Trim leading and trailing whitespace
char *
trim_user_ent (char *buf) {
	char *s, *e;

	// Find first non-whitespace character
	for (s = buf; isspace (*s); s++);

	// Backup over any trailing whitespace
	for (e = s + strlen(s) - 1; e >= s && isspace (*e); e--);
	e++;

	if (e > s) {
		*e = '\0';
		if (s != buf)
			strcpy (buf, s);
	} else
		*buf = '\0';

	return buf;
}
