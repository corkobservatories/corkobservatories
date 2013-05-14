 /* mldat9.c formats 2008 binary MLCork data whether parosci -1 or parosci-2
 	gauge is being used, and whether temperature measurement is thermistor 
	type or platinum chip type.
 	 simple linear temperature calibration
	 for platinum chip to give a true temperature (in degrees C) output.

	The binary input format is

					item	  #bytes    format
					----    ------    ------
					time		 4			epochal time from Jan.1 1988
					ident		 1			unique id for RTC
					temp		 3			platinum chip temperature measurement
               data		 4			p period for each ppc
					byte		 1			zero byte (if not in Neptune mode)

	Output files are

	.dat	Formatted data with header info RE: requested data format generation
	.err	Erroneous bytes in .bin file not written to the .dat file because
			a) the data sample was not the right number of bytes
			b) the time of the sample was out of sequence
			c) the bytes were not data samples
	.flg 	Flag file contains all info printed to the screen during run time
	.spk	Spike log contains the time of spike, spike value, and replacement

	Created Oct 3, 2008
	Modified  Nov 9, 2008    */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
//#include "commlib.h"
//#include "asciidef.h"
//#include "ibmkeys.h"

#define ESC 27
#define CR 13
#define LF 10

int opnfilna(FILE **pFile,char **nP,char *mode,char *pName,char *ext);
void wrt2dat(FILE *pW, long time0, unsigned long temp, unsigned long pp[][2], int ns, int therm);
void wrt2dat_2(FILE *pW, unsigned long time0,unsigned long temp, unsigned long tp[][2], unsigned long pp[][2], int ns, int therm);
void wrthead(FILE *pW, char *rawname, int ns);
int initialize_vals(int therm);
int wrt2err(FILE *pR, FILE *pE,long nchars, int ns);
int lost(FILE *pR, long *fposit,unsigned long time0);
unsigned long read32(void);
unsigned long read8(unsigned long);
int hamali(unsigned long pp[][2], unsigned long *ps, int ns);
void slog(FILE *pL, unsigned long pp[][2], unsigned long *ps, int ns);
int hamali_2(unsigned long tp[][2], unsigned long *ts, unsigned long pp[][2], unsigned long *ps, int ns);
void slog_2(FILE *pL, unsigned long tp[][2], unsigned long *ts, unsigned long pp[][2], unsigned long *ps, int ns);
void closeallf(void);
int getint (int min, int max);
double getfloat (void);
char *trim (char *buf);

//long get(int n);
//long serial;long Tcorr=0L;
double celcius = 0; 
double temp_usec[8] = {0,0,0,0,0,0,0,0};
//char units[5][7]={"\0","usec","kHz","kPa,C"};
//extern int ns;
FILE *pR,*pW,*pE,*pF,*pL;
int tform;
int format;

//could potentially add an id byte check after the time check if need be
int 
main (int argc, char **argv) {
	char *Usage = "Usage: %s [bin filename] \n";
//	static int nb;				// number of bytes in sample
	int ns;						// number of pressure channels
	unsigned long pp[8][2];
	unsigned long ps[8];
	unsigned long tp[8][2];
	unsigned long ts[8];
	unsigned long time0,tmp,time1=0;
	unsigned long temp;
	unsigned char id,byte;
	char *pName;
	int i;
	long intvl,maxintvl=3600;
	long int n=0L;
	char Bin[MAXPATHLEN];
	static char Working[MAXPATHLEN];		// working filename for future use
	long e,nchars;
	char text[80];
	int abort=0;
	char goodt[256];
	long fposit,fposita;
	int k,nsp=0;
	int rtn;
	char *cc;
	char c[256];
	int paroTemp;
	int therm;

	/* print splash for program */
    printf("\n\r\n\rMLDAT binary to ascii conversion \nfor 2007 and later MTO1/PPC systems   %s\n\r", __DATE__);
	if (argc == 2) {
		pName = argv[1];
		pR = fopen(pName, "rb");
		if (!pR) {
			perror(pName);
			exit(1);
		}
		printf("Opened %s\n", pName);
	}
	else if (argc == 1) {
		printf("\n\n.bin ");
		if(opnfilna(&pR,&pName,"rb",NULL,"bin")) exit(1);  // quit if no entry
	}
	else {
		fprintf(stderr, Usage, argv[0]);
		exit(1);
	}
	strcpy(Bin,pName);			// save .bin filename in Bin 
	strcpy(Working,pName);// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL) *cc = '\0';

	/* open .dat file */
	printf(".dat ");
	if(opnfilna(&pW,&pName,"wt",Working,"dat")){	// problem opening file
		closeallf();				// close input
		return 1;
	}
	strcpy(Working,pName);// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL) *cc = '\0';

	/* open .err (error) file */
	printf(".err ");
	if(opnfilna(&pE,&pName,"w+",Working,"err")){
		closeallf();				// close input
		return 1;
	}
	strcpy(Working,pName);// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL) *cc = '\0';

	/* open .flg (flag) file */
	printf(".flg ");
	if(opnfilna(&pF,&pName,"w+",Working,"flg")){
		closeallf();				// close input
		return 1;
	}
	strcpy(Working,pName);// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL) *cc = '\0';

	/* open .spk (despike log) file */
	printf(".spk ");
	if(opnfilna(&pL,&pName,"w+",Working,"spk")){
		closeallf();				// close input
		return 1;
	}
	strcpy(Working,pName);// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL) *cc = '\0';


	printf("\n Are the paroscientifics making a temperature measurement? (y/n): ");
	fflush(stdout);
	fgets(c, sizeof(c), stdin);
	c[0] = tolower(c[0]);
	while(!(c[0] == 'y' || c[0] == 'n')){
		printf("\ny/n?");
		fflush(stdout);
		fgets(c, sizeof(c), stdin);
		c[0] = tolower(c[0]);
	}
	if(c[0] == 'n') paroTemp = 0;
	else paroTemp = 1;
	
	printf("\n Is the PPC temperature measurement made with a platinum chip? (y/n): ");
	fflush(stdout);
	fgets(c, sizeof(c), stdin);
	c[0] = tolower(c[0]);
	while(!(c[0] == 'y' || c[0] == 'n')){
		printf("\ny/n?");
		fflush(stdout);
		fgets(c, sizeof(c), stdin);
		c[0] = tolower(c[0]);
	}
	if(c[0] == 'n') therm = 1;
	else therm = 0;

	if (paroTemp == 0) {
		printf("\nSelect desired temperature and pressure format...");
		printf("\n  0. Bennest thermometer counts and pressure period in hexadecimal");
		printf("\n  1. B. therm. counts and pressure period in usec");
		printf("\n  2. B. therm. in degrees C and frequency in kHz");
		printf("\n  3. user supplied temperature for pressure compensation");
		printf("\n  4. Bennest thermometer compensated pressure\n");
		printf("\n  and the choice is... ");
    }
	else {
		printf("\nSelect desired temperature and pressure format...");
		printf("\n  0. Bennest thermometer counts and parosci periods in hexadecimal");
		printf("\n  1. B. therm. counts in decimal and parosci periods in usec");
		printf("\n  2. B. therm.  in degrees C and parosci frequencies in kHz");
		printf("\n  3. user supplied temperature for pressure compensation");
		printf("\n  4. Parosci temperature compensated pressure\n");
		printf("\n  and the choice is... ");
    }
	fflush(stdout);
	format = getint (0, 5);
	//printf("format is %d\n", format);

	if (format >= 2){
		if ((rtn = initialize_vals(therm)) == -1) {
			printf("One of the necessary coefficient files (parosci.txt, platinum.txt, or therms.txt) couldn't be found.\n");
			closeallf();
			return 1;
		}
		ns = rtn;
		if (format==3) {
			if (!paroTemp) {
				printf("\n\nEnter single temp (øC) for compensating P: ");
				fflush(stdout);
				celcius = getfloat();
			} else {
				printf("\n\nEnter temp (in usec) for compensating pressure\n ");
				for(i = 0; i<ns; i++) {
					printf("channel %d: ",i+1);
					fflush(stdout);
					temp_usec[i] = getfloat();
				}
			}
		}
	}
	else ns = 8;
	printf("\n\n Select the format for time...\n");
	printf("  1. YYMMDDhhmmss   \n");
	printf("  2. hex\n  3. no time\n\n  going for... ");
	fflush(stdout);
	tform = getint(1,3);

	printf("\n\n");

	/* Check for and eliminate initial zero. */
	fread(&byte,sizeof(byte),1, pR);
	if(byte==0){
		printf("\nInitial zero byte removed.");
	}
	else rewind(pR);
	time0=read32();
	e=time0+567993600L;   // adjust offset to Jan.1, 1970
	strftime(text, 17, "%y%m%d%H%M%S", gmtime(&e));
	printf("\nTime 0: %s \n Is this reasonable?",text);
	fgets(goodt,sizeof(goodt), stdin);
	while(!(goodt[0] == 'y' || goodt[0] == 'n')){
		printf("\ny/n?");
		fflush(stdout);
		fgets(goodt,sizeof(goodt), stdin);
		goodt[0]=tolower(goodt[0]);
	}
	if(goodt[0]=='n'){
		abort=1;
		printf("\nfix .bin file so it starts with a good time.");
	}
	ns=ns+1;               // +1 to account for reading in the next time
	while(!feof(pR) && !abort){      // stay here until EOF or ESC gets us out
		++n;
		printf("\r%8lx, %d spikes",time0,nsp);
		if(n==1){ // find next time incrementally.
			temp=read32();
			id=temp>>24;
			for(i=0;i<ns;i++){			// go to max PPCs +1 (9)
				tmp=read32();
				if((tmp>time0)&&(tmp<=time0+1800)){ // case: starts in neptune, no 00
					time1=tmp;
					intvl=time1-time0;
					ns=i;
				}
				else if(((tmp<<8)-(time0&0xffffff00))<=4096){  //if looking like it is a time
					time1=read8(tmp);
					intvl=time1-time0;
					ns=i;
				}
				else{
					if (paroTemp){
						tp[i][0]=tmp;
						pp[i][0]=read32();
					} else
						pp[i][0]=tmp;
				}
			}//end for(i=0;i<ns;i++)
			if(time1==0 || intvl>maxintvl || intvl < 0){
				printf("\n Problem with data format. Be sure you've chosen\n a site with the right number of pressure gauges.");
				printf("\n ns is equal to %d.",ns);
				printf("\n %ld second sample interval.\n",intvl);
				printf("\nTime 0: %s, %8lx",text, time0);
				e=time1+567993600L;   // adjust offset to Jan.1, 1970
				strftime(text, 17, "%y%m%d%H%M%S", gmtime(&e));
				printf("\nTime 1: %s, %8lx",text, time1);
				return 1;
			}
			else{
				printf("\nTime 0: %s",text);
				fprintf(pF,"\nTime 0: %s",text);
				e=time1+567993600L;   // adjust offset to Jan.1, 1970
				strftime(text, 17, "%y%m%d%H%M%S", gmtime(&e));
				printf("\nTime 1: %s",text);
				fprintf(pF,"\nTime 1: %s",text);
				printf("\n %d pressure channels detected.",ns);
				printf("\n %d bytes per sample.", ns*4+8);
				printf("\n %ld second sample interval.\n",intvl);
				fprintf(pF,"\n %d pressure channels detected.",ns);
				fprintf(pF,"\n %d bytes per sample.", ns*4+8);
				fprintf(pF,"\n %ld second sample interval.\n",intvl);
			}
			wrthead(pW, Bin, ns);
			if (paroTemp)
				wrt2dat_2(pW, time0, temp, tp, pp, ns, therm);
			else
				wrt2dat(pW, time0, temp, pp, ns, therm);
			time0=time1;
		}   //end first sample read and format detection


		else{           			//read data following time0 assuming okay
			fposit = ftell(pR); //save position of after the last good time
			temp = read32();
			for(i=0;i<ns;i++){
				if (paroTemp){
					tp[i][1]=read32();
					pp[i][1]=read32();
				}else
					pp[i][1]=read32();
			}

			tmp=read32();          //next time assuming w/o 00

			//read potential ID byte & reset pointer
			fposita = ftell(pR);
			fread(&byte, sizeof(byte),1,pR);
			fseek(pR,fposita, SEEK_SET);

			if((tmp>time0)&&byte==id){ //if looking like it is time w/o 00
				time1=tmp;
				if(intvl!=(time1-time0)&&(time1-time0)<=1800){
					intvl=time1-time0;
					printf("\nNew sample interval = %ld\n", intvl);
					fprintf(pF,"%8lx, %d spikes",time0,nsp);
					fprintf(pF,"\nNew sample interval = %ld\n", intvl);
				}
				else if((time1-time0)>1800&&(time1-time0)<=3600){
					printf("\nPotential download in 1Hz, time gap= %ld seconds.\n",time1-time0);
					fprintf(pF,"%8lx, %d spikes",time0,nsp);
					fprintf(pF,"\nPotential download in 1Hz, time gap= %ld seconds.\n",time1-time0);
				}
				if (paroTemp)
					k = hamali_2(tp,ts,pp,ps,ns);
				else
					k = hamali(pp,ps,ns);
				if(k != 0){
					nsp=nsp+k;
					fprintf(pL,"%8lx",time0);	// write time
					if (paroTemp)
						slog_2(pL, tp, ts, pp, ps, ns);
					else
						slog(pL,pp,ps,ns);
				}
				for(i=0;i<ns;i++) {
					pp[i][0]=pp[i][1];
					if (paroTemp)
						tp[i][0] = tp[i][1];
				}
				if (paroTemp)
					wrt2dat_2(pW, time0, temp, tp, pp, ns, therm);
				else
					wrt2dat(pW, time0, temp, pp, ns, therm);
				time0=time1;
			}

			//if looking like it is time with 00
			else if(((tmp<<8)-(time0&0xffffff00))<=4096){
				time1=read8(tmp);
				if(time1-time0!=intvl){
					if((time1-time0)<=maxintvl&&time1>time0){
						intvl=time1-time0;
						printf("\nNew sample interval = %ld\n", intvl);
						fprintf(pF,"%8lx, %d spikes",time0,nsp);
						fprintf(pF,"\nNew sample interval = %ld\n", intvl);
						if (paroTemp)
							k = hamali_2(tp,ts,pp,ps,ns);
						else
							k = hamali(pp,ps,ns);
						if(k != 0){
							nsp=nsp+k;
							fprintf(pL,"%8lx",time0);	// write time
							if (paroTemp)
								slog_2(pL, tp, ts, pp, ps, ns);
							else
								slog(pL,pp,ps,ns);
						}
						for(i=0;i<ns;i++) {
							pp[i][0]=pp[i][1];
							if (paroTemp)
								tp[i][0] = tp[i][1];
						}
						if (paroTemp)
							wrt2dat_2(pW, time0, temp, tp, pp, ns, therm);
						else
							wrt2dat(pW, time0, temp, pp, ns, therm);
						time0=time1;
					}
					else{
						if(time1-time0==0){     //this is a special case
							if (paroTemp)
								k = hamali_2(tp,ts,pp,ps,ns);
							else
								k = hamali(pp,ps,ns);
							if(k != 0){
								nsp=nsp+k;
								fprintf(pL,"%8lx",time0);	// write time
								if (paroTemp)
									slog_2(pL, tp, ts, pp, ps, ns);
								else
									slog(pL,pp,ps,ns);
							}
							for(i=0;i<ns;i++) {
								pp[i][0]=pp[i][1];
								if (paroTemp)
									tp[i][0] = tp[i][1];
							}
							//data pre time glich gets written
							if (paroTemp)
								wrt2dat_2(pW, time0, temp, tp, pp, ns, therm);
							else
								wrt2dat(pW, time0, temp, pp, ns, therm);
							printf("\nRepeat sample. We're lost \n");
							fprintf(pF,"%8lx, %d spikes",time0,nsp);
							fprintf(pF,"\nRepeat sample. We're lost\n");
							//fposit = ftell(pR); //pointer after time glich
							fposita = fposit;
							if(lost(pR,&fposit,time0)<0){   //Change later to write to err to EOF
								//will only enter if exit(0) from read8 & read32 removed
								printf("\nReached end of file in lost");
								fprintf(pF,"\nReached end of file in lost");
								closeallf();
								return 1;
							}
							nchars = fposit-fposita;
							printf(" Writing %ld to error file. \n", nchars);
							fprintf(pF," Writing %ld to error file. \n", nchars);
							fseek(pR,fposita, SEEK_SET);
							if(wrt2err(pR, pE,nchars,ns)<0){
								//will only enter if exit(0) from read8 & read32 removed
								closeallf();
								return 1;
							}

							time0=read32();
						}
						else{
							if(time1>time0){
								printf("\nPotential download, time gap= %ld seconds.\n",time1-time0);
     				 			fprintf(pF,"%8lx, %d spikes",time0,nsp);
								fprintf(pF,"\nPotential download, time gap= %ld seconds.\n",time1-time0);
								if (paroTemp)
									k = hamali_2(tp,ts,pp,ps,ns);
								else
									k = hamali(pp,ps,ns);
								if(k != 0){
									nsp=nsp+k;
									fprintf(pL,"%8lx",time0);	// write time
									if (paroTemp)
										slog_2(pL, tp, ts, pp, ps, ns);
									else
										slog(pL,pp,ps,ns);
								}
								for(i=0;i<ns;i++) {
									pp[i][0]=pp[i][1];
									if (paroTemp)
										tp[i][0] = tp[i][1];
								
								}
								if (paroTemp)
									wrt2dat_2(pW, time0, temp, tp, pp, ns, therm);
								else
									wrt2dat(pW, time0, temp, pp, ns, therm);
								time0=time1;
							}
							else{
								printf("\nTime gone backwards. We're lost.\n");
								fprintf(pF,"%8lx, %d spikes",time0,nsp);
								fprintf(pF,"\nTime gone backwards. We're lost.\n");
								fposita = fposit;
								if(lost(pR,&fposit,time0)<0){ //Change later to write to err to EOF
									//will only enter if exit(0) from read8 & read32 removed
									printf("\nReached end of file in lost\n");
									fprintf(pF,"\nReached end of file in lost\n");
									closeallf();
									return 1;
								}
								nchars = fposit - fposita;
								printf(" Writing %ld to error file.\n", nchars);
								fprintf(pF," Writing %ld to error file.\n", nchars);
								fseek(pR,fposita, SEEK_SET);
								if(wrt2err(pR,pE,nchars,ns)<0){
									//will only enter if exit(0) from read8 & read32 removed
									closeallf();
									return 1;
								}
								time0=read32();
							}
						}
					}//end else : it is time, but messed up in some form or another
				}//end if time1-time0!=interval with 00
 				else{
					if (paroTemp)
						k = hamali_2(tp,ts,pp,ps,ns);
					else
						k = hamali(pp,ps,ns);
					if(k != 0){
						nsp=nsp+k;
						fprintf(pL,"%8lx",time0);	// write time
						if (paroTemp)
							slog_2(pL, tp, ts, pp, ps, ns);
						else
							slog(pL,pp,ps,ns);
					}
					for(i=0;i<ns;i++) {
						pp[i][0]=pp[i][1];
						if (paroTemp)
							tp[i][0] = tp[i][1];
					}
					if (paroTemp)
						wrt2dat_2(pW, time0, temp, tp, pp, ns, therm);
					else
						wrt2dat(pW, time0, temp, pp, ns, therm);
					time0=time1;
				}
			} else{
				printf("\nNot a time value. We're lost.\n");
				fprintf(pF,"%8lx, %d spikes",time0,nsp);
				fprintf(pF,"\nNot a time value. We're lost.\n");
				fposita = fposit;
				if(lost(pR,&fposit,time0)<0){   //Change later to write to err to EOF
					printf("\n Reached end of file in lost");
					fprintf(pF,"\n Reached end of file in lost");
					closeallf();
					return 1;
				}
				nchars = fposit - fposita;
				printf(" Writing %ld to error file\n", nchars);
				fprintf(pF," Writing %ld to error file\n", nchars);
				fseek(pR,fposita, SEEK_SET);
				if(wrt2err(pR,pE,nchars,ns)<0){
					//will only enter if exit(0) from read8 & read32 removed
					closeallf();
					return 1;
				}
				time0=read32();
			}
		} //end else not first sample
	}//end while
	closeallf();
	return 0;
}


int
getint (int min, int max) {
	char buf[BUFSIZ];
	char *ep;
	int rtn;

	for (;;) {
		fflush(stdout);
		fgets (buf, sizeof (buf), stdin);
		trim (buf);
		if (*buf) {
			rtn = (int) strtol (buf, &ep, 10);
			if (!*ep && (rtn >= min) && (rtn <= max))
				return rtn;
		}
		printf ("\nInvalid entry (%s). Try again: ", buf);
	}
}

double
getfloat () {
	char buf[BUFSIZ];
	char *ep;
	double rtn;

	for (;;) {
		fflush(stdout);
		fgets (buf, sizeof (buf), stdin);
		trim (buf);
		if (*buf) {
			rtn = strtod (buf, &ep);
			if (!*ep)
				return rtn;
		}
		printf ("\nInvalid entry (%s). Try again: ", buf);
	}
}

// Trim leading and trailing whitespace
char *
trim (char *buf) {
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
