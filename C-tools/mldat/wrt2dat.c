#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

#define ESC 27
#define CR 13
#define LF 10

int initialize_vals(int therm);
int getint (int min, int max);
double getfloat (void);
int getsn(int nTsensor, int nParo);
int findTsensor(char *Tsensor, int nTsensor);
int findParo(long int Paro, int nParo, int j);
int platinum(void);
int therms(void);
int Parosci(void);
double caliplat(double temp, int i);
double calitherm(double temp, int i);
double calitemp(double tper, double *Tc, int i); 
double calipress_2(double u, double pper, int i); 
double calipress(double temperature, double pper, int i);
//unsigned long bytes2long(const unsigned char *str, int len); //used in mltermlin formdataspit, but not here where read32 and read8 have already been used.
extern char *trim (char *buf);

extern int kfd;
static long int SNA[100]; // All the serial numbers. Actual serial numbers to be applied in calibration are SNA[SNi]
static char tSNA[100][3];
extern int tform;
extern int format;
extern double celcius;
extern double temp_usec[8];

static int nChan;
static int tSNi;
static int SNi[8];
static int format_is_init = 0;

//platinum coeffs:
static double slope[100],intercept[100];
//therm coeffs:
double A[100],B[100],C[100],X1[100],X2[100],X3[100], X4[100], X5[100],X6[100];
//parosci coeffs:
static double U0[100],Y1[100],Y2[100],C1[100],C2[100],C3[100];
static double D1[100],T1[100],T2[100],T3[100],T4[100];

void wrt2dat(FILE *pW, long time0, unsigned long temp,
	unsigned long pp[][2], int ns, int therm)
{
	char text[24];
	double ppd;
	double Pc,Tc;
	int i,id;
	
	switch(tform){
		case 1:
			time0=time0+567993600L;    // adjust offset to Jan.1, 1970
			strftime(text, 17, "%y%m%d%H%M%S", gmtime(&time0));
			fprintf(pW,"%s",text);
			//printf("%s",text);
			break;

		case 2:
			fprintf(pW,"%8lx",time0);
			//printf("%8lx",time0);
			break;

		case 3:
			break;

		case ESC:
			return;
	}
	id = temp>>24;
	fprintf(pW," %2x",id);

	temp = temp&0x00ffffff;

	for (i=0;i<ns;i++) {
		// print pressure and temperature data
		switch(format) {			// selected format
			case 0:// original hex periods without 2^32
				if (i == 0) fprintf(pW," %6lx",temp);
				fprintf(pW," %8lx",pp[i][0]);
				//printf(" %8lx",pp[i][0]);
				break;

			case 1:	// logger counts and true period (25us=140000000h)
				if (i == 0) fprintf(pW," %8ld",temp);
				if (pp[i][0] == 0xffffffff) ppd = -1;
				else {
					ppd=((double)pp[i][0]+4294967296.L)*4.656612873E-15L;
					ppd=ppd*1e6;
				}
				fprintf(pW," %0.10lf",ppd);
				break;

			case 2:		//true temperature and true frequency
				
				if (!format_is_init) 
					printf("Coefficients for temperature sensor or pressure gauges not loaded\n");
				if (i == 0) {
				//printf("\n%ld",tSNA[tSNi]);
					if (tSNA[tSNi] != 0) {
						if (therm)
							Tc = calitherm((double)temp,tSNi);
						else 
							Tc = caliplat((double)temp,tSNi);
						fprintf(pW," %0.4lf",Tc);
					}
					else fprintf(pW," %8ld",temp);
				}
				if( pp[i][0] == 0xffffffff) ppd = -1;
				else {
					ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-15L;
					ppd = 1/ppd/1000.;
				}
				fprintf(pW," %10lf",ppd);
				break;
			case 3:     // uses T value entered to compensate Ps

				if (!format_is_init) 
					printf("Coefficients for temperature sensor or pressure gauges not loaded\n");
				if (i == 0) {
				//printf("\n%ld",tSNA[tSNi]);
					if (tSNA[tSNi] != 0){
						if (therm)
							Tc = calitherm((double)temp,tSNi);
						else 
							Tc = caliplat((double)temp,tSNi);
						fprintf(pW," %4lf",Tc);
					}
					else fprintf(pW," %8ld",temp);
				}
				ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-9L;
				Pc = calipress(celcius, ppd, SNi[i]);
				fprintf(pW, " %5lf", Pc);
				break;	
			case 4: // uses PPC thermometer for compenstating Ps

				if (!format_is_init) 
					printf("Coefficients for temperature sensor or pressure gauges not loaded\n");
				if(tSNA[tSNi] == 0){
					printf("\nPPC thermometer compensated pressure not valid for thermometer %s.",tSNA[tSNi]);
					exit(1);
				}
				if(i == 0){
					if (therm)
						Tc = calitherm((double)temp,tSNi);
					else 
						Tc = caliplat((double)temp,tSNi);
					fprintf(pW," %4lf",Tc);
				}
				ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-9L;
				Pc = calipress(Tc,ppd,SNi[i]);
				fprintf(pW," %5lf",Pc);
				break;
		}//end switch format
	}//end for i<ns
	fprintf(pW,"\n");
}

void wrt2dat_2(FILE *pW, long time0, unsigned long temp, unsigned long tp[][2], unsigned long pp[][2], int ns, int therm)
{
	char text[24];
	double tpd, ppd;
	double Pc, Tc, T;
	int i,id;
	
	switch(tform){
		case 1:
			time0=time0+567993600L;    // adjust offset to Jan.1, 1970
			strftime(text, 17, "%y%m%d%H%M%S", gmtime(&time0));
			fprintf(pW,"%s",text);
			break;

		case 2:
			fprintf(pW,"%8lx",time0);
			break;

		case 3:
			break;

		case ESC:
			return;
	}
	id=temp>>24;
	fprintf(pW," %2x",id);

	temp=temp&0x00ffffff;

	for (i=0; i<ns; i++){
		// print pressure and temperature data
		switch(format) {			// selected format
			case 0:		// original hex periods without 2^32
				if (i == 0) fprintf(pW," %6lx",temp);
				fprintf(pW," %8lx %8lx",tp[i][0],pp[i][0]);
				break;

			case 1:		// logger counts and true period (25us=140000000h)
				if (i == 0) fprintf(pW," %8ld",temp);
				if (tp[i][0] == 0xffffffff) tpd = -1;
				else {
					tpd = ((double)tp[i][0]+4294967296.L)*4.656612873E-15L;
					tpd = tpd*1e6/4;
				}																	 
				
				if (pp[i][0] == 0xffffffff) ppd = -1;
				else {
					ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-15L;
					ppd = ppd*1e6;
				}
				fprintf(pW," %0.10lf %0.10lf", tpd, ppd);
				break;

			case 2:		//true temperature and true frequency
				
				if (!format_is_init) 
					printf("Coefficients for temperature sensor or pressure gauges not loaded\n");
				if(i==0){
				//printf("\n%ld",tSNA[tSNi]);
					if (tSNA[tSNi] != 0){
						if (therm)
							T = calitherm((double)temp,tSNi);
						else 
							T = caliplat((double)temp,tSNi);
						fprintf(pW," %4lf",T);
					}
					else fprintf(pW," %8ld",temp);
				}
				if (tp[i][0] == 0xffffffff) tpd = -1;
				else {
					tpd = ((double)tp[i][0]+4294967296.L)*4.656612873E-15L;
					tpd = 1/tpd/1000.*4;
				}
				if (pp[i][0] == 0xffffffff) ppd = -1;
				else {
					ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-15L;
					ppd = 1/ppd/1000.;
				}
				fprintf(pW," %10lf %10lf",tpd, ppd);
				break;
			case 3:     // user supplied Tc value (in usecs) entered to compensate Pc

				if (!format_is_init) 
					printf("Coefficients for temperature sensor or pressure gauges not loaded\n");
				if (i == 0) {
				//printf("\n%ld",tSNA[tSNi]);
					if (tSNA[tSNi] != 0) {
						if (therm)
							T = calitherm((double)temp,tSNi);
						else 
							T = caliplat((double)temp,tSNi);
						fprintf(pW," %4lf",T);
					}
					else fprintf(pW," %8ld",temp);
				}
				tpd = (((double)tp[i][0]+4294967296.L)*4.656612873E-9L)/4;
				calitemp(tpd, &Tc, SNi[i]);
				ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-9L;
				Pc = calipress_2(calitemp(temp_usec[i], NULL, SNi[i]),ppd,SNi[i]);
				fprintf(pW," %5lf %5lf", Tc, Pc);
				break;	
			case 4: // uses parosci temperature(Tc) for compensating pressure
	
				if (!format_is_init) 
					printf("Coefficients for temperature sensor or pressure gauges not loaded\n");
				if(i == 0) {
				//printf("\n%ld",tSNA[tSNi]);
					if(tSNA[tSNi] != 0) {
						if (therm)
							T = calitherm((double)temp,tSNi);
						else 
							T = caliplat((double)temp,tSNi);
						fprintf(pW," %4lf",T);
					}
					else fprintf(pW," %8ld",temp);
				}
				tpd = (((double)tp[i][0]+4294967296.L)*4.656612873E-9L)/4;
				ppd = ((double)pp[i][0]+4294967296.L)*4.656612873E-9L;
				Pc = calipress_2(calitemp(tpd, &Tc, SNi[i]),ppd,SNi[i]);
				fprintf(pW," %5lf %5lf",Tc, Pc);
				break;						
		}//end switch format
	}//end for i<ns
	fprintf(pW,"\n");
}


void wrthead(FILE *pW, char *Raw, int ns)
{
	time_t ltime;
   int i;

	time(&ltime);
      fprintf(pW,".dat file created from %s",Raw);
		fprintf(pW," on %s", ctime(&ltime));
		fprintf(pW,"Output format selected:");
	      switch(format){			// selected format
         	case 0:// original hex periods without 2^32
				fprintf(pW,"\n 0. temp. counts and pressure period in hexadecimal.");
				break;

			case 1:	// logger counts and true period (25us=140000000h)
				fprintf(pW,"\n 1. decimal temp. counts and pressure period (usec).");
				break;

            case 2:		//true temperature and true frequency
				fprintf(pW,"\n 2. temperature in degrees C and frequency in kHz.");
				fprintf(pW,"\nCoefficients for Bennest thermometer number %s used.", tSNA[tSNi]);
				break;

            case 3:   // uses T value entered to compensate Ps
				fprintf(pW,"\n 3. user supplied temperature for compensation.");
				fprintf(pW,"\nCoefficients for Bennest thermometer number %s used.", tSNA[tSNi]);
				if (celcius)
					fprintf(pW,"\nUser supplied temperature was %4lf degrees C.", celcius);
				else {
					fprintf(pW,"\nUser supplied temperature in usecs:\n");
					for (i = 0;i<ns; i++){
						fprintf(pW, "%10lf",temp_usec[i]);
						if(i!=ns-1) fprintf(pW,", ");
					}
				}
				fprintf(pW,"\nParosci coeffs used were those associated with gauges:\n ");
				for(i=0;i<ns; i++){
					fprintf(pW,"%ld", SNA[SNi[i]]);
					if(i!=ns-1) fprintf(pW,", ");
				}
     			break;
	 		case 4: // uses thermistor T for compenstating Ps	
				fprintf(pW,"\n 4. Temperature compensated pressure.");
				fprintf(pW,"\nCoefficients for Bennest thermometer number %s used.", tSNA[tSNi]);
				fprintf(pW,"\nParosci coeffs used were those associated with gauges:\n ");
				for(i=0;i<ns; i++){
					fprintf(pW,"%ld", SNA[SNi[i]]);
					if(i!=ns-1) fprintf(pW,", ");
				}
				break;

			} //end switch
		fprintf(pW,"\n\n");
}

int initialize_vals(int therm) {
	int nTsensor, nParo;
	
	if(!format_is_init) {
		if(!(nParo = Parosci()) || (!therm && !(nTsensor = platinum())) || (therm && !(nTsensor = therms())) ) {
			printf("parosci.txt or platinum.txt or therms.txt not found\n\r");
			fflush(stdout);
			return -1;
		}
		nChan = getsn(nTsensor, nParo); 
		format_is_init = 1;
	}
	return nChan;
}


int getsn(int nTsensor, int nParo){
	/* getsn.c prompts the user for sensor(thermometer and pressure) serial nos */
	int i,j,k; //i for total sensor number, j for pressure channel, k for Site, 
	int nChan;
	int nSites = 0;
	int ret[9] = {1,1,1,1,1,1,1,1,1};
	char name[30][10],date[30][10],buff[80]; //loaded from sites.txt
	long int x[30][9];						 //press gauge SN from sites.txt
	//int t[30];
	char t[30][3];								 //thermometer SN from sites.txt
	int nsens[30];  //number of pressure channels for each site in sites.txt
	FILE *pSites;
	static long int Paro;
	static char Tsensor[256];
	//unsigned char queue4sensor[9][30] = 
	printf("\n\rSerial numbers are required.\n\rSelect from site list:\n\r");
	
	pSites = fopen("sites.txt","rt");
	if(pSites) {
		/* read sites.txt file of holes and Parosci numbers */
		fgets(buff,sizeof(buff),pSites);	// 1st line
		fgets(buff,sizeof(buff),pSites);	// 2nd line
		fgets(buff,sizeof(buff),pSites);	// 3rd and blank line
		memset (t, 0, sizeof (t));
		memset (x, 0, sizeof (x));
		for(k=0;k<30;k++){  //there has to be a faster way.  All of these loops take forever!
			fgets(buff,sizeof(buff),pSites);
			if(feof(pSites)!=0) break;

			sscanf(buff,"%s %s %s %ld %ld %ld %ld %ld %ld %ld %ld %ld",
				name[k],date[k],t[k],&x[k][0],&x[k][1],&x[k][2],&x[k][3],
				&x[k][4],&x[k][5],&x[k][6],&x[k][7],&x[k][8]);
			for(j=0;j<9;j++){
				if(x[k][j]==0L) break;
			}
			nsens[k]=j;
			printf("%d: %s",k+1,buff);
		}
		nSites = k;
		printf("\n\ror press '0' for manual entry.");
		fflush(stdout);

	}
	else {
		printf("\n\rUnable to open sites.txt");
		printf("\n\rpress '0' for manual entry.");
		fflush(stdout);
	}
	k = getint(0,nSites);

	if(k>0){
		nChan=nsens[k-1];
		//printf("\nnChan is %d",nChan);
		printf("\n\rUsing sensor list for %s.\n\r",name[k-1]);
		strcpy (Tsensor, t[k-1]); //why don't I have to trim string t[k-1]
		ret[0] = findTsensor(Tsensor,nTsensor);
		for(j=0;j<nChan;j++){
			Paro=x[k-1][j];
			//if(Paro==0){  //x is prefilled with zeroes, but we don't run over ns (filled) so this shouldn't ever get called.
			//	break;
			//}
			if(format>=3){ 
				ret[j+1] = findParo(Paro, nParo, j);
			}
			else ret[j+1] = 0; //for pressure data in frequency no need to have valid parosci numbers.
		} //end for j<nChan
	}
	else{
		printf("\n\r Manual entry. How many Paroscientifics? ");
		fflush(stdout);
		nChan = getint(0,8);
	}
	// get manual entry of serial number and look in files for serial numbers (from sites.txt or manual entry)
	for (i = 0; i < nChan+1; i++) {
		while (ret[i]) {
			if (i == 0) printf("\n\rEnter thermometer serial number\n\r");
			else if (i == 1) {
				printf("\n\rEnter serial nos");
				printf("\n\rsensor #%d...",i);
			}
			else
				printf("\n\rsensor #%d...",i);

			fflush(stdout);
			
			if (i == 0) {
				fgets (Tsensor, sizeof (Tsensor), stdin);
				ret[i] = findTsensor(trim (Tsensor),nTsensor);
			}
			else {
				Paro = getint(0,1e6);
				ret[i] = findParo(Paro, nParo, i-1);
			}
		}//end while
	}//end for look in files for manual entered 
	return nChan;
}


int 
findTsensor(char *Tsensor, int nTsensor) {
	int i;	
	for(i=0;i<nTsensor;i++){
		if (!strcasecmp (Tsensor, tSNA[i])) {
			tSNi=i;
			printf("Coeffs for Tsensor %s will be used for formating\n", tSNA[tSNi]); 
			//printf("\nindex is %d, serial number is %d",tSNi,tSNA[tSNi]);
			return 0;
		}
	}
	printf("            %s is not in the file!\n",Tsensor);
	return 1;
}

int findParo(long int Paro, int nParo, int j) {
	int i;
	for(i=0;i<nParo;i++){
		if(Paro == SNA[i]){
			SNi[j] = i;
			printf("Coeffs for Paro %ld will be used for formating \n", SNA[SNi[j]]); 
			return 0;
		}
	}//end for
	printf("            %ld is not in the file!\n",Paro);
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
		printf("\n\rUnable to open platinum.txt, the coefficient file\n\r");
		return 0;
	}
	
	i=0;
	while (1) {
		fscanf (tTsensor, "%*s%s\n", buf);
		if (feof(tTsensor)){
			fclose(tTsensor);
			break;
		}
		strncpy (tSNA[i], trim (buf), sizeof (tSNA[i]));
		//tSNA[i][sizeof (tSNA[i]) - 1] = '\0';
		fscanf(tTsensor,"%*s%lf\n%*s%lf\n", &slope[i],&intercept[i]);
		i++;
	}
	nTsensor = i;
	printf("\n\r<<<Platinum chip coeffs loaded for %d sensors>>>\n\r", nTsensor);
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
		strncpy (tSNA[i], trim (buf), sizeof (tSNA[i]));
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

	pParo=fopen("parosci.txt","rt");
	if (pParo == NULL) {
		printf("\n\rUnable to open parosci.txt, the coefficient file\n\r");
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
	printf("\n\r<<<Paroscientific coeffs loaded for %d sensors>>>\n\r", nParo);
	return nParo;
}

double caliplat(double temp, int i){
	/* caliplat.c determine temp in deg C from temp counts and platinum chip s/n */
	/* temp = temperature
	i = sensor index number  (ie tSNi) */
	double Tc;
	
	Tc=slope[i]*temp + intercept[i];
	return Tc;
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
												  
double calitemp(double tper, double *Tc, int i) {
	static double u,u2;

	// get temperature in deg C, tc
	u=tper-U0[i];
	u2=u*u;
	if (Tc)
		*Tc=Y1[i]*u+Y2[i]*u2;
	return u;
}

double calipress_2(double u, double pper, int i) {
	// now get pressure and convert from psia to kPa
	static double Pc,u2,c,d,t0,tr;
	
	u2 = u*u;
	c=C1[i]+C2[i]*u+C3[i]*u2;
	t0=T1[i]+T2[i]*u+T3[i]*u2+T4[i]*u2*u;
	tr=1-t0/pper*t0/pper;

	d=D1[i];
	Pc=c*tr*(1-d*tr);
	Pc=6.894757*Pc;     // psi to kPa
	return Pc;
}



double calipress(double celcius, double pper, int i){
	/*	calipress.c compensated pressure from thermistor,platinum, paro temp,  or user temp and paro s/n */
	/*	celcius = temperature in deg C
		pper = pressure period (usec)
		i = sensor index number (ie SNi)  */

	static double Pc,u,u2,c,d,t0,tr;

	// temperature in degrees C from thermistor, platinum chip, parosci or user entry
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

unsigned long bytes2long(const unsigned char *str, int len) {
	unsigned long val = 0;

	while (len--) {
		val <<= 8;
		val += *str++;
	}
	return val;
}

/*
void spitforminfo(FILE *pLog, int dataformat) {
	switch (dataformat) {
	case 0:
		printf ("Hex Strings\n\r");
		fflush(stdout);
		if (pLog) fprintf (pLog, "Hex Strings\r\n");
		break;
	case 1:
		printf ("time, temp (oC), temperature compensated pressures (kPa)\n\r");
		fflush(stdout);
		if (pLog) fprintf (pLog, "time, temp (oC), temperature compensated pressures (kPa)\r\n");
		break;
	}
	return;
}
*/
