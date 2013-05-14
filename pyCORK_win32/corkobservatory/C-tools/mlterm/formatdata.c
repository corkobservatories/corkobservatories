#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/timeb.h>
#include <ctype.h>
#include <math.h>

#include "mltermfuncs.h"
#include "specialkeys.h"
#include "ttyfuncdecs.h"
#include "usermenu.h"

#define INTERVAL 500
#define NUMDATAFORM 2

extern int ifd, kfd, nfds;
static char tSNA[100][3]; // All the serial numbers. Actual serial numbers to be applied in calibration are tSNA[tSNi]
static long int SNA[100];
static int nChan;
static int tSNi;
static int SNi[9];
static int format_is_init = 0;
static int therm;
//platinum coeffs:
static double slope[100],intercept[100];
//therm coeffs:
double A[100],B[100],C[100],X1[100],X2[100],X3[100], X4[100], X5[100],X6[100];
//parosci coeffs
static double U0[100],Y1[100],Y2[100],C1[100],C2[100],C3[100];
static double D1[100],T1[100],T2[100],T3[100],T4[100];

void formatdata(char *sitename, int high) {
	int j;
	int len;
	int ret;
	int bytecount = 0;
	int userentry;
	int data_format;
	unsigned char *dataspitp;
	unsigned char dataspit[16384]; 
	unsigned char cc[8192];
	char ch[512];
	char chUP;
	struct timeb previous;
	fd_set rfds;
	struct timeval tv;
	const MenuInfo *mi;
	
	Debug("Turning data formatting on.\n");
	log_printf(LOG_SCREEN_MAX, "Turning data formatting on.\n");
	if (high) {
		log_printf(LOG_SCREEN_MIN, "Please be patient...\n");	
		data_format = 2;
		TalkLstn(1);
		fasttalk((unsigned char *) "Q");
		clearserialbuf();
	}
	else
		data_format = 0;

	ftime (&previous);
	
	if (data_format > 1) { //this has to also be inside while loop, because when running mltermlin, data_format gets changed inside while loop
		if (!format_is_init) {
			if (!(mi = get_mt01_info(1, 0))) {
				Debug("menu parse failed.\n");
				return;
			}
			if (strstr(mi->version, "2.1.1") || strstr(mi->version, "2.2.0")) {
				Debug("Detected instrument is old version: %s\n", mi->version);
				therm = 1;
			} else {
				Debug("Detected instrument is new: %s\n", mi->version);
				therm = 0;
			}
			ret = initialize_vals(sitename);// only read in coeff files if format requires it.	
			if (ret) {
				if (!high) { 
					Debug("Serial number entry failed, revert to data format 0\n");
					data_format = 0;
				} else {
					Debug("Serial number entry failed, revert to data format 1\n");
					data_format = 1;
				}
			}
			fasttalk((unsigned char *) "Q");
			clearserialbuf();
		}
	}
	log_printf(LOG_SCREEN_MIN,"\nWaiting for sample from instrument..\n");
	spitforminfo(data_format);
	dataspitp = dataspit;
	raw(kfd, 0);	
	while(1) {		
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		FD_ZERO (&rfds);
		FD_SET (ifd, &rfds);
		FD_SET (kfd, &rfds);
		if (select (nfds, &rfds, NULL, NULL, &tv) == -1) {
			if (errno != EINTR) {
				if (high) {
					invalidate_mt01_cache();
					TalkLstnRestore();
				}
				return;
			} else {
				fasttalk((unsigned char *) "Q");
			}
		}
		else if (FD_ISSET (ifd, &rfds)) {
			// Bail if our buffer is full and there's still more to read
			if (bytecount == sizeof (dataspit)) {
				if (high) {
					invalidate_mt01_cache();
					TalkLstnRestore();
				}
				return;
			}
			if ((len = readDebug (ifd, dataspitp, sizeof(dataspit) - bytecount, "instrument")) < 0) {
				perror ("read");
				restore(1);
			}
			dataspitp += len;
			bytecount = dataspitp - dataspit;  //bytecount and dataspitp may seem repetative, but bytecount changes at the end (if lstnmode = ASC)
		} else if (FD_ISSET (kfd, &rfds)){
			if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
				perror ("read");
				restore(1);
			}
			userentry = isspecialkey(cc, len);
			if (userentry == FORM_DATALOW && !high) {
				if (data_format != NUMDATAFORM) {
					data_format++; 
					if (data_format > 1) {
						if (!format_is_init) {
							if (!(mi = get_mt01_info(1, 0))) {
								Debug("menu parse failed.\n");
								return;
							}
							if (strstr(mi->version, "2.1.1") || strstr(mi->version, "2.2.0")) {
								Debug("Detected instrument is old version: %s\n", mi->version);
								therm = 1;
							} else {
								Debug("Detected instrument is new: %s\n", mi->version);
								therm = 0;
							}
							restore_tty(kfd);
							ret = initialize_vals(sitename);// only read in coeff files if format requires it.	
							raw(kfd,0);	
							if (ret) {
								if (!high) { 
									Debug("Serial number entry failed, revert to data format 0\n");
									data_format = 0;
								} else {
									Debug("Serial number entry failed, revert to data format 1\n");
									data_format = 1;
								}
							}
							fasttalk((unsigned char *) "Q");
							clearserialbuf();
						}
					}
				}
				else data_format = 0;
				spitforminfo (data_format);
/*
			} else if (userentry == OPEN_LOG) {
				Debug("log file\n");
				log_open();
*/
			} else if (userentry == HELP_MENU && !high) { 
				log_printf(LOG_SCREEN_MIN, "\n--accessing MLTERM help --\n");
				Debug("--accessing MLTERM help --\n");
				help();
				printf("\n");
			} else if (userentry == PAUSE) {
				log_printf(LOG_SCREEN_MIN, "Pausing data print to screen..(hit any key to resume)\n");
				errno = 0;
				do {
					if (errno == EINTR) fasttalk((unsigned char *) "Q");
					FD_ZERO (&rfds); 
					FD_SET (kfd, &rfds);
				} while ((select(kfd+1,&rfds, NULL, NULL, NULL) < 0) && (errno == EINTR));
				if ((len = readDebug (kfd, cc, sizeof(cc), "keyboard")) < 0) {
					perror ("read");
					restore(1);
				}
				log_printf(LOG_SCREEN_MIN, "Resuming data print to screen.\n");
				clearserialbuf();
			} else if (userentry == QUITLOW) {
				if (format_is_init) {
					log_printf (LOG_SCREEN_MIN, "Exiting format data. Save site and/or serial no. formatting info? (y/n) ");
					Debug ("Exiting format data. Save?\n");
					tcflush(kfd, TCIFLUSH);
				} else {
					log_printf (LOG_SCREEN_MIN, "Exiting format data.\n");
					Debug ("Exiting format data.\n");
				}
				if (high) { //in high mode, alarm is still on to keep instrument awake
					restore_tty(kfd);
					if (format_is_init) {
						fgets(ch,sizeof(ch),stdin);
						Debug("User responded %c to 'Save?'\n",ch[0]);
						chUP = toupper (ch[0]);
						if (chUP != 'Y') format_is_init = 0;
					}			
					if (get_mt01_info(1,1) == NULL) //instrument shouldn't need waking but check anyway.
						wake(1,'k');
					TalkLstnRestore();
				} else if (format_is_init) {
					readDebug(kfd, ch, 1, "keyboard");
					log_printf(LOG_SCREEN_MIN, "%c\n", ch[0]);
					Debug("User responded '%c' 'Save?'\n",ch[0]);
					chUP = toupper (ch[0]);
					if (chUP != 'Y') format_is_init = 0;
				}
				return;
			} else
				log_printf(LOG_SCREEN_MIN, "Hit <ESC> to quit data format\n");
		} else { //tv time out
				if (dataspitp != dataspit) {
					Debug("bytecount at end of data sentence = %d\n", bytecount);
					if (isnhex((char *) dataspit,bytecount-2)) {
						dataspitp = dataspit;
						for(j=0;j<bytecount;j+=2){
							if (isnhex((char *) &dataspit[j], 2)) //this will drop off the non hex <cr><lf>
								*dataspitp++ = hex2int((char *) &dataspit[j]);
							
						}
						bytecount = dataspitp - dataspit;
						Debug("New bytecount = %d\n", bytecount);
					}
					dataspitp = dataspit; // must reset here
					DebugBuf(bytecount, dataspit, "dataspit in binary to be sent out for formatting\n");
					Debug("data_format = %d\n", data_format);
					if (bytecount && bytecount < 66 && dataspit[0] != (unsigned char) 'Q') formdataspit(dataspit, bytecount, data_format,sitename, high);
				}
				//previous = now;
		}
		tv.tv_usec = 100000;
		tv.tv_sec = 0;
	}//end while
}

void formdataspit(unsigned char *dataspit, int bytecount, int data_format, char *sitename, int high) {
	int i;
	char text[24]; 
	long time;
	unsigned long epoch, temp, pp[8], tp[8];
	int ID;
	double T, Tc, Pc, tpd, ppd;
	switch (data_format) {
		case 0:
			for(i=0; i < bytecount; i++){
				log_printf(LOG_SCREEN_MIN, "%02X",dataspit[i]);
			}
			log_printf(LOG_SCREEN_MIN, "\n");
			break;
		case 1:
			// TIME
			epoch = bytes2long(&dataspit[0], 4);
			ID = dataspit[4];
			time=epoch+567993600L;           // adjust offset to Jan.1, 1970
			strftime(text, 17, "%H:%M:%S %d%b%y", gmtime(&time));
			log_printf(LOG_SCREEN_MIN, "%s %02X ",text, ID);
			for(i=5; i < bytecount; i++){
				if ((i+1)%4 == 1)
					log_printf(LOG_SCREEN_MIN, " ",dataspit[i]);
				if ((bytecount - i == 1) && (bytecount%4 == 1)) //removes trailing 00 byte if there is one
					break;
				log_printf(LOG_SCREEN_MIN, "%02X",dataspit[i]);
			}
			log_printf(LOG_SCREEN_MIN, "\n");
			break;
		case 2:
			// TIME
			epoch = bytes2long(&dataspit[0], 4);
			ID = dataspit[4];
			temp = bytes2long(&dataspit[5], 3);
			//for (i = 0; 8 + i*4 < bytecount - 3; i++) 
			for (i = 0; i < nChan; i++) {
				if (!U0[SNi[i]]) {
					pp[i] = bytes2long(&dataspit[8 + i*4], 4); 
				} else {
					tp[i] = bytes2long(&dataspit[8 + i*8], 4); 
					pp[i] = bytes2long(&dataspit[12 + i*8], 4); 
					//log_printf(LOG_SCREEN_MIN, "%08X %08X",tp[i], pp[i]);
				}	
			//if (bytecount > 8 + i*4 + 3)  //works for DONET and VENUS
				if (!U0[SNi[i]]) {
					if (bytecount != 8 + nChan*4 +1) 
						log_printf(LOG_SCREEN_MIN, "Data sentence has unexpected number of bytes for %d pressure channels. \n", nChan);
				} else {
					if (bytecount != 8 + nChan*8 +1) 
						log_printf(LOG_SCREEN_MIN, "Data sentence has unexpected number of bytes for %d pressure channels. \n", nChan);
				}
			}
			time=epoch+567993600L;           // adjust offset to Jan.1, 1970
			strftime(text, 17, "%H:%M:%S %d%b%y", gmtime(&time));
			if (therm) 
				T=calitherm((double)temp,tSNi);
			else 
				T=caliplat((double)temp,tSNi);
			log_printf(LOG_SCREEN_MIN, "%s %02X %0.4lf",text, ID, T);
			//Debug("Temp coeff = %lf (if not 0 than paro-2)\n", U0[SNi[i]]);
			for (i = 0; i < nChan; i++) {
				if (!U0[SNi[i]]) {
					ppd=((double)pp[i]+4294967296.L)*4.656612873E-9L; //for mldat8 change pp[i] back to pp[i][0]
					Pc=calipress(T,ppd,SNi[i]);
					log_printf(LOG_SCREEN_MIN, " %0.5lf",Pc);
				} else {
					tpd = (((double)tp[i]+4294967296.L)*4.656612873E-9L)/4;
					ppd = ((double)pp[i]+4294967296.L)*4.656612873E-9L;
					Pc = calipress_2(calitemp(tpd, &Tc, SNi[i]),ppd,SNi[i]);
					log_printf(LOG_SCREEN_MIN, " %0.5lf %0.5lf", Tc, Pc);
					//log_printf(LOG_SCREEN_MIN, " %10lf %10lf",tpd, ppd); 
				}
			}
			
			log_printf(LOG_SCREEN_MIN, "\n");
			break;
		}
}


int initialize_vals(char *sitename) {
	int nTsensor, nParo;
	int ret;
	if(!format_is_init) {
		if(!(nParo = Parosci()) || (!therm && !(nTsensor = platinum())) || (therm && !(nTsensor = therms())) ) {
			log_printf(LOG_SCREEN_MIN, "parosci.txt or platinum.txt or therms.txt not found\n");
			return 2;
		}
		ret = getsn(nTsensor, nParo, sitename);
		clearserialbuf();
		if (ret) return ret;
		format_is_init = 1;
	}
	return 0;
}


int getsn(int nTsensor, int nParo, char *sitename){
	/* getsn.c prompts the user for sensor(platinum chip and pressure) serial nos */
	int i,j,k; //i for total sensor number, j for pressure channel, k for Site, 
	int nSites;
	int ret[9] = {1,1,1,1,1,1,1,1,1};
	char name[30][12],date[30][12],buff[80]; //loaded from sites.txt
	long int x[30][9];						 //press gauge SN from sites.txt
	char t[30][3];								 //platinum temp gauge SN from sites.txt
	int nsens[30];  //number of pressure channels for each site in sites.txt
	FILE *pSites;
	char userentry[20];
	static long int Paro;
	static char Tsensor[256];
	//unsigned char queue4sensor[9][30] = 

	if (!sitename) log_printf(LOG_SCREEN_MIN, "\nSerial numbers are required.\nSelect from site list:\n");
	Debug("processing sites.txt\n");
	pSites=fopen("sites.txt","rt");
	if(pSites) {
		/* read sites.txt file of holes and Parosci numbers */
		fgets(buff,80,pSites);	// 1st line
		fgets(buff,80,pSites);	// 2nd line
		fgets(buff,80,pSites);	// 3rd and blank line
		memset (t, 0, sizeof (t));
		memset (x, 0, sizeof (x));
		for(k=0;k<30;k++){  //there has to be a faster way.  All of these loops take forever!
			fgets(buff,80,pSites);
			if(feof(pSites)!=0) break;

			sscanf(buff,"%s %s %s %ld %ld %ld %ld %ld %ld %ld %ld %ld",
				name[k],date[k],t[k],&x[k][0],&x[k][1],&x[k][2],&x[k][3],
				&x[k][4],&x[k][5],&x[k][6],&x[k][7],&x[k][8]);
			for(j=0;j<9;j++){
				if(x[k][j]==0L) break;
			}
			nsens[k]=j;
			if (!sitename) log_printf(LOG_SCREEN_MIN, "%d: %s",k+1,buff);
		}
		fclose (pSites);
		Debug("done processing sites.txt\n");
		nSites = k;
		if (!sitename) log_printf(LOG_SCREEN_MIN, "\npress '0' for manual entry or <enter> to skip serial no. entry.");
	}
	else {
		nSites = 0;
		log_printf(LOG_SCREEN_MIN, "\nUnable to open sites.txt\npress '0' for manual entry or <enter> to skip serial no. entry.");
		Debug("Unable to open sites.txt: errno=%d\n", errno);
	}

	if (!sitename) {
		tcflush(kfd, TCIFLUSH);
		k = getint(0,nSites);
		//termln(userentry,sizeof(userentry));	
		//DebugBuf(strlen(userentry), (unsigned char *) userentry, "user entry\n");
		//if (userentry[0] == ESC || userentry[0] == 0x11) return 1;
		//if (sscanf(userentry,"%d",&k) != 1)
		//	k = -1;
	}

	else {
		Debug("sitename = %s\n", sitename);
		for(k=0; k<nSites;k++) {
			Debug("name in line %d = %s\n", k+1, name[k]);
			if (strcasecmp(sitename, name[k]) == 0) {
				break;
			}
		}
		if (k==nSites) k=0;
		else k++; //this is to make k match with the k that a user would enter from the menu (no site has k=0)

	}
	
	if (k<0 || k>nSites) {
		Debug("\ninvalid entry or user chose to skip serial no. entry\n");
		return -1;
	}

	if(k>0){
		nChan=nsens[k-1];
		log_printf(LOG_SCREEN_MIN, "\nUsing sensor list for %s.\n",name[k-1]);
		Debug("\nUsing sensor list for %s.\n",name[k-1]);
		strcpy (Tsensor, t[k-1]);	
		ret[0] = findTsensor(Tsensor,nTsensor);
		for(j=0;j<nChan;j++){
			Paro=x[k-1][j];
			//if(format>=3){ //I may use this for linux version of mldat8
			ret[j+1] = findParo(Paro, nParo, j);	
		} //end for j<nChan
	}
	else{
		log_printf(LOG_SCREEN_MIN, "\n Manual entry. How many Paroscientifics? ");
		//readDebug(kfd, &ch, 1, "keyboard");
		nChan = getint(0,8);
	//	Debug("user entry: %\n", ch);
	//	log_printf(LOG_SCREEN_MIN, "%c\n", ch);
	//	if (ch == 0x1B || ch == 0x11) {
	//		log_printf(LOG_SCREEN_MIN, "escape from enter num Paros. \n");
	//		Debug("escape from enter num Paros. (getsn) \n");
	//		return -1;
	//	}
	//	log_printf(LOG_SCREEN_MIN, "\n");
	//	nChan = ch - '0';
		if (nChan<0 || nChan >8) {
			log_printf(LOG_SCREEN_MIN, "\ninvalid entry\n");
			Debug("invalid entry\n");
			return -1;
		}
	}
	// get and look in files for manual entered serial numbers
	for (i = 0; i < nChan+1; i++) {
		while (ret[i]) {
			if (i == 0) {
				log_printf(LOG_SCREEN_MIN, "\nEnter thermometer number\n");
				Debug("Enter thermometer number\n");
			} else if (i == 1) {
				log_printf(LOG_SCREEN_MIN, "\nEnter serial nos\n");
				log_printf(LOG_SCREEN_MIN, "sensor #%d...",i);
			}
			else
				log_printf(LOG_SCREEN_MIN, "sensor #%d...",i);

			fflush(stdin);
			if (i == 0) {
				termln(userentry,sizeof(userentry));	
				DebugBuf(strlen(userentry), (unsigned char *) userentry, "user entry\n");
				
				if (isspecialkey( (unsigned char *) userentry,1)==QUITLOW) return 1;
				//if (userentry[0] == 0x1B || userentry[0] == 0x11) return 1;
				//sscanf(userentry,"%d",&Tsensor); this worked prior to platinums being named in hex
				ret[i] = findTsensor(trim_user_ent (userentry),nTsensor);
			}
			else {
				termln(userentry,sizeof(userentry));	
				DebugBuf(strlen(userentry), (unsigned char *) userentry, "userentry\n");
				if (isspecialkey( (unsigned char *) userentry,1)==QUITLOW) return 1;
				//if (userentry[0] == 0x1B || userentry[0] == 0x11) return 1;
				sscanf(userentry,"%ld",&Paro);
				ret[i] = findParo(Paro, nParo, i-1);	
			}
		}//end while
	}//end for look in files for manual entered 
	return 0;
}


int findTsensor(char *Tsensor, int nTsensor) {
	int i;	
	for(i=0;i<nTsensor;i++){
		if (!strcasecmp (Tsensor, tSNA[i])) {
			tSNi=i;
			log_printf(LOG_SCREEN_MIN, "Coeffs for Tsensor %s will be used for formating \n", tSNA[tSNi]); 
			Debug("Coeffs for Tsensor %s will be used for formating \n", tSNA[tSNi]); 
			return 0;
		}
	}
	log_printf(LOG_SCREEN_MIN, "            %s is not in the file!\n",Tsensor);
	return 1;
}


int findParo(long int Paro, int nParo, int j) {
	int i;
	for(i=0;i<nParo;i++){
		if(Paro == SNA[i]){
			SNi[j] = i;
			log_printf(LOG_SCREEN_MIN, "Coeffs for Paro %d will be used for formating \n", SNA[SNi[j]]); 
			Debug("Coeffs for Paro %d will be used for formating \n", SNA[SNi[j]]); 
			return 0;
		}
	}//end for
	log_printf(LOG_SCREEN_MIN, "            %ld is not in the file!\n",Paro);
	return 1;
}

int platinum(void){
/* Tsensors.c loads the platinum chip coefficients from the text
	file platinum.txt to the coefficient arrays.  */
	char buf[256];
	int i;
	int nTsensor;  //number of temperature sensors in planitum.txt coeffs file
	FILE *tTsensor;

	tTsensor = fopen("platinum.txt","rt");
	if (tTsensor == NULL) {
		log_printf(LOG_SCREEN_MIN, "\nUnable to open platinum.txt, the coefficient file\n");
		return 0;
	}

	i=0;
	while (1) {
		fscanf (tTsensor, "%*s%s\n", buf);
		if (feof(tTsensor)){
			fclose(tTsensor);
			break;
		}
		strncpy (tSNA[i], trim_user_ent (buf), sizeof (tSNA[i]));
		//tSNA[i][sizeof (tSNA[i]) - 1] = '\0';
		fscanf(tTsensor,"%*s%lf\n%*s%lf\n", &slope[i],&intercept[i]);
		i++;
	}
	nTsensor = i;
	log_printf(LOG_SCREEN_MIN, "\n<<<Platinum chip coeffs loaded for %d sensors>>>\n", nTsensor);
	return nTsensor;
}

int therms(void){
/* therms.c loads the thermistor and board coefficients from the text
	file therms.txt to the coefficient arrays.  */
	int i;
	char buf[256];
	int nTsensor;
	FILE *tTsensor;

	tTsensor=fopen("therms.txt","rt");
	if(tTsensor==NULL){
		printf("\nUnable to open therms.txt, the coefficient file\n");
		return 0;
	}
	i=0;
	while(1){
		fscanf(tTsensor,"%*s%s\n",buf);
		if(feof(tTsensor)){
			fclose(tTsensor);
			break;
		}
		strncpy (tSNA[i], trim_user_ent (buf), sizeof (tSNA[i]));
		//tSNA[i][sizeof (tSNA[i]) - 1] = '\0';
		fscanf(tTsensor,"%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n",
			&A[i],&B[i],&C[i],&X1[i],&X2[i],&X3[i],&X4[i],&X5[i],&X6[i]);
		i++;
	}
	nTsensor = i;
	printf("\n<<<Thermistor and board coeffs loaded for %d sensors>>>\n\n",nTsensor);
	return nTsensor;
}

int Parosci(void){
	/* Parosci.c loads the paroscientific coefficients from the text
		file parosci.txt to the coefficient arrays.  */
	int i;
	int nParo;
	long int sn;
	FILE *pParo;

	Debug("processing parosci.txt\n");
	pParo=fopen("parosci.txt","rt");
	if (pParo == NULL) {
		log_printf(LOG_SCREEN_MIN, "\nUnable to open parosci.txt, the coefficient file\n");
		return 0;
	}
	i = 0;
	while (1) {
		fscanf(pParo,"%*s%ld\n",&sn);
		if (feof(pParo)){
			fclose(pParo);
			break;
		}
		SNA[i] = sn;
		fscanf(pParo,"%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n",
							&U0[i],&Y1[i],&Y2[i],&C1[i],&C2[i],&C3[i]);
		fscanf(pParo,"%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n%*s%lf\n",
					 &D1[i],&T1[i],&T2[i],&T3[i],&T4[i]);
		i++;
	}
	nParo = i;
	log_printf(LOG_SCREEN_MIN, "\n<<<Paroscientific coeffs loaded for %d sensors>>>\n", nParo);
	Debug("done processing parosci.txt\n");
	return nParo;
}

double caliplat(double temp, int i){
	/* caliplat.c determine temp in deg C from temp counts and platinum chip s/n */
	/* temp = temperature
	   i = sensor index number  (ie tSNi) */
	double T;
	T=slope[i]*temp + intercept[i];
	return T;
}

double calitherm(double temp, int i){
	/* temp = temperature in hex
	i = sensor index number   */
	double Tmp,Tc,m;
	static double r, lnr,lnr3, invT;
	
	r=X3[i]*(temp-X1[i])/(X2[i]-X4[i]*(temp-X1[i]));
	lnr=log(r);
	lnr3=lnr*lnr*lnr;
	invT=A[i]+B[i]*lnr+C[i]*lnr3;
	Tmp=1.00L/invT-273.15L;
	m=(X5[i]-X6[i])/(X6[i]-25);
	Tc=Tmp+Tmp*m-25*m;
	return Tc;
}

double calipress(double celcius, double pper, int i){
	/*	calipress.c compensated pressure from thermistor or user temp and paro s/n */
	/*	celcius = temperature in deg C
		pper = pressure period (usec)
		i = sensor index number (ie SNi)  */

	static double Pc,u,u2,c,d,t0,tr;

	// temperature in øC from platinum chip or user entry
	u=celcius;
	u2=u*u;

	// now get pressure and convert from psia to kPa
	c=C1[i]+C2[i]*u+C3[i]*u2;
	d=D1[i];
	t0=T1[i]+T2[i]*u+T3[i]*u2+T4[i]*u2*u;

	tr=1-t0/pper*t0/pper;
	Pc=c*tr*(1-d*tr);
	Pc=6.894757*Pc;		// psi to kPa
	return Pc;
}

												  
double calitemp(double tper, double *Tc, int i) {
	static double u,u2;

	// get temperature in deg C, tc
	// Debug("tpd is %10lf",tper);
	u=tper-U0[i];
	u2=u*u;
	*Tc=Y1[i]*u+Y2[i]*u2;
	return u;
}

double calipress_2(double u, double pper, int i) {
	// now get pressure and convert from psia to kPa
	static double Pc,u2,c,d,t0,tr;
	
	// Debug("ppd is %10lf, u is %10lf",pper, u);
	u2 = u*u;
	c=C1[i]+C2[i]*u+C3[i]*u2;
	t0=T1[i]+T2[i]*u+T3[i]*u2+T4[i]*u2*u;
	tr=1-t0/pper*t0/pper;

	d=D1[i];
	Pc=c*tr*(1-d*tr);
	Pc=6.894757*Pc;     // psi to kPa
	return Pc;
}




unsigned long bytes2long(const unsigned char *str, int len) {
	unsigned long val = 0;

	while (len--) {
		val <<= 8;
		val += *str++;
	}
	return val;
}


void spitforminfo(int dataformat) {
	log_printf(LOG_SCREEN_MIN,"\n");
	switch (dataformat) {
	case 0:
		Debug ("Hex Strings\n");
		log_printf (LOG_SCREEN_MIN, "Hex Strings\n");
		break;
	case 1:
		Debug ("Formatted time\n");
		log_printf (LOG_SCREEN_MIN, "Formatted time   ID Therm  Paros..\n");
		break;
	case 2:
		Debug("Temp coeff = %lf (if not 0 than paro-2)\n", U0[SNi[0]]);
		if (!U0[SNi[0]]) {
			Debug ("time, ID, temp (oC), temperature compensated pressures (kPa)\n");
			log_printf (LOG_SCREEN_MIN, "time, ID, temp(oC), temperature compensated pressures(kPa)\n");
		} else {
			Debug ("time, ID, case temp (oC), gauge temp (oC), compensated pressure(kPa)\n");
			log_printf (LOG_SCREEN_MIN, "time, ID, case temp(oC), gauge temp(oC), compensated pressure(kPa)\n");
		}
		break;
	}
	return;
}
