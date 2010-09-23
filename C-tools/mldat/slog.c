/* slog.c   logs spike values */

#include <stdio.h>


void slog(FILE *pL, unsigned long pp[][2], unsigned long *ps, int nch)
{
	int i;
	for(i=0;i<nch;i++){
/*    if(tp[i][1]==tp[i][0]){	// spike replaced
			fprintf(pL,";%.8lx;%.8lx",ts[i],tp[i][1]);
		}
	else{
*/
		fprintf(pL,";;");                   //no temperature values replaced
//		}
	 	if(pp[i][1]==pp[i][0]){		// replaced value
			fprintf(pL,";%.8lx;%.8lx",ps[i],pp[i][1]);
		}
		else{
			fprintf(pL,";;");
		}
	 }
	 fprintf(pL,"\r\n");
}

