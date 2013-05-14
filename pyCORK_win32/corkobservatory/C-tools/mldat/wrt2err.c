#include <stdio.h>
#include <ctype.h>
#include <errno.h>

int wrt2err(FILE *in, FILE *out, long nchars, int ns){
	int i,cc;
	int isasc;
	long fposita;
	int smpsize;

	fprintf(out,"\nWriting %ld erroneous bytes.\n",nchars);

	smpsize=ns*4+4;		//not plus 8 because pointer to file is after time.
//	smpsize=smpsize-4;		//pointer to file is after time, so subtract time bytes
	if(nchars<smpsize)
		isasc=0;
	else{
		fposita = ftell(in);
		isasc=1;
		for(i=0;i<(smpsize+8);i++){
			if((cc=fgetc(in))<0)
				return -1;
			else if(i<smpsize)
         	fprintf(out," %2X",cc);
			else{
				if(!isascii(cc)){
					isasc=0;
					break;
				}
			}
		}//end for loop
		fprintf(out,"\n");
		fposita = fposita + smpsize;
		nchars = nchars - smpsize;
   		fseek(in, fposita, SEEK_SET);
	}

	while(nchars-->0){
		if((cc=fgetc(in))<0)
			return -1;
		else if(isasc)
			fputc(cc,out);
		else
			fprintf(out," %2X",cc);
	}
	return 0;
}
