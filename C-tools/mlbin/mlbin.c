/*

	The 512 byte packets stored in compact flash are formatted as
	
			[soh][LBA][------data------][crc]
		
		name  bytes		definition
		soh  	1			0x01 (start of header)
		LBA  	3			Low 3 bytes of compact flash logical block address
		data  506		ML-CORK data
		crc  	2			CRC-16 of data section

	When data are dumped from the MT01 serial to compact flash logger
	using the sector based protocol, the sector block format is
	preserved and must be removed before the data can be used.

	In addition to error free packets, the *.raw file contains packets
	received with crc errors and such packets have the soh character
	replaced by stx, 0x02.  Bad packets are retransmitted a second
	time, and the second copy could be good.  In the event it is, the
	first copy is discarded.  If the second packet is also bad, then
	a byte-wise comparison is made between the two, in an attempt to
	build a third packet which satisfies the crc.  Should this fail, the
	best guess version of the packet is saved in the output file.

	This program generates a .bin file from the .raw file.

*/

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include "crc.h"

#define SOH 1
#define STX 2

#define FNDX_RAW	0
#define FNDX_BIN	1
#define FNDX_ERR	2

//int opnfil(FILE **pFile, char *mode);
int opnfilna(FILE **pFIle, char **nP, char *mode,char *pName,char *ext);
void stats(void);

int Ncrc=0;			// number of sector errors
int Nsoh=0;			// number of soh errors
int Nlba=0;			// number of lba errors
int Nret=0;			// number of times retry saved the day
int Nfix=0;			// # times a right was made from two wrongs
int Nfail=0;		// failed to fix bad packet
int Nodd=0;

char Raw[MAXPATHLEN];
char Files[3][MAXPATHLEN];	// filenames for directory list at end
char Working[MAXPATHLEN];		// working filename for future use
char *pName;
unsigned char Buf1[512], Buf2[512], Buf3[512];
unsigned fMap;				// where are we?
int Bad[512];				// bad byte index
FILE *pIn,*pE,*pOut;			//pointers to disk files: raw, err, dat

int 
main (int argc, char **argv) {
	char *Usage = "Usage: %s [raw file name] \n";
	int i,j,m,k,nk=0;
	long int lastadrs=0;		// first LBA
	long int nblk=0;
	long int adrs;
	int nbad=0;
	long int ii,kk,bb;
	char *cc;
	
	/* print splash for program */
	printf("\nMLBIN 1.0:  converts SCF downloaded .raw to .bin %s\n\r", __DATE__);
	fflush(stdout);

	/* open download file */
	if (argc == 2) {
		pName = argv[1];
		pIn=fopen(pName,"rb");
		if(!pIn) {
			perror(pName);
			exit(1);
		}
		printf("Opened %s\n", pName);
		fflush(stdout);
	}
	else if (argc == 1) {
		printf("\n\n.raw ");
		if(opnfilna(&pIn,&pName,"rb",NULL,"raw"))
			return 1;  // quit if no entry
	}
	else {
		fprintf(stderr,Usage,argv[0]);
		exit(1);
	}
		
	strcpy(Raw,pName);			// save .raw filename in Raw
	strcpy(Files[FNDX_RAW],pName);
	strcpy(Working,pName);	// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL)
		*cc = '\0';

	/* open .bin file */
	printf(".bin ");
	if(opnfilna(&pOut,&pName,"wb",Working,"bin")){	// problem opening file
		fclose(pIn);				// close input
		fclose(pE);					// and error files
		return 3;
	}
	strcpy(Files[FNDX_BIN],pName);
	strcpy(Working,pName);	// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL)
		*cc = '\0';
	
	/* open .err (error) file */
	printf(".err ");
	if(opnfilna(&pE,&pName,"w+",Working,"err"))
		return 2;
	strcpy(Files[FNDX_ERR],pName);
	strcpy(Working,pName);	// copy name to working string
	if ((cc = strrchr(Working,'.')) != NULL)
		*cc = '\0';
		
	printf("\n");

	fMap=0;					// starting out
	
	while(1){
		fread(Buf1,sizeof(char),512,pIn);
		if(feof(pIn)){
			stats();
			fclose(pIn);
			fclose(pE);
			fclose(pOut);
			fflush(stdout);
			return 0;
		}
		// here we have a block
		nblk++;
		switch(Buf1[0]){
			case 0x01:	// soh: blk was downloaded ok
				if(cal_crc(&Buf1[4], 508)==0){	// crc
					for(k=0;k<508;k++){
						if(Buf1[k]==0x20 && Buf1[k+1]==0xab && Buf1[k+2]==0x9f){
							nk++;
						}
					}
					fwrite(&Buf1[4],sizeof(char),506,pOut);
					SAVE:
					adrs=(Buf1[1]*256L+Buf1[2])*256L+Buf1[3];
					printf("\rblk=%ld  LBA=%ld",nblk,adrs);
					lastadrs=adrs;
					continue;			// this is the good loop
				}
				else{			// soh was ok but crc was wrong...VERY unlikely
					fprintf(pE,"\nBlk %ld LBA %ld?: soh but crc bad, saved.",nblk,adrs);
					Nodd++;
					goto SAVE;
				}
				break;

			case 0x02:	// stx: blk had error on download
				fread(Buf2,sizeof(char),512,pIn);	// read the single retry
				if(feof(pIn)){		// oops
					fprintf(pE,"\nBlk %ld LBA %ld?: EOF on retry block, saved original",
								nblk,adrs);
					goto SAVE;
					break;
				}
				if(cal_crc(&Buf2[4],508)==0){	// crc
					fwrite(&Buf2[4],sizeof(char),506,pOut);
					fprintf(pE,"\nBlk %ld LBA %ld: retry good, saved",nblk,adrs);
					Nret++;
					goto SAVE;
				}

				/* okay, we have two bad copies of the data:( */
				nbad=0;
				for(i=0;i<512;i++){
					if(Buf1[i]==Buf2[i]){	// if bytes are same in both blocks...
						Buf3[i]=Buf1[i];		// keep 'em
					}
					else{
						Bad[nbad]=i;			// keep track of bad ones
						nbad=nbad+1;
					}
				}
				fprintf(pE,"\nBlk %ld LBA %ld: two bad blocks %d bad bytes...",
						nblk,adrs,nbad);

				if(nbad>16)
					fprintf(pE,"Skipping.");
				else{
					// make use a binary counter to select all 2^nbad
					//	combinations of different bytes
					bb=1;
					for(j=0;j<nbad;j++){
						bb=bb*2L;
					}
					for(ii=0L;ii<bb;ii++){
						kk=1;
						for(j=0;j<nbad;j++){
							m=Bad[j];
							if(ii&kk){
								Buf3[m]=Buf2[m];
							}
							else{
								Buf3[m]=Buf1[m];
							}
							kk=kk<<1;
						}
						if(cal_crc(&Buf3[4],508)==0){
							fprintf(pE,"\nBlk %ld LBA %ld: fixed and saved",nblk,adrs);
							Nfix++;
							fwrite(&Buf3[4],sizeof(char),506,pOut);
							goto SAVE;
						}
					}
					fprintf(pE,"\nBlk %ld LBA %ld: Fix failed, skipping.",nblk,adrs);
				}
				break;
		}
	}
}


void stats(void){
	extern int Ncrc,Nsoh,Nlba,Nfix;
	int n;

	n=0;
	printf("\n");
	if(Ncrc!=0){
		printf("\n%d crc errors",Ncrc);
		n=1;
	}
	if(Nsoh!=0){
		printf("\n%d soh errors",Nsoh);
		n=1;
	}
	if(Nlba!=0){
		printf("\n%d LBA errors",Nlba);
		n=1;
	}
	if(Nret!=0){
		if(Nret==1)
			printf("\n%d successful retry",Nret);
		else
			printf("\n%d successful retries",Nret);
		n=1;
	}
	if(Nfix!=0){
		printf("\n%d successful fix",Nfix);
		n=1;
	}
	if(!n){
		printf("\nNo Errors!");
		unlink(Files[FNDX_ERR]);
	}
	else{
		printf("\nSee %s for details\n",Files[FNDX_ERR]);
	}
	printf("\n");
}
