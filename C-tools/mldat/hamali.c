/* hammer.c   the spike detector and fixer-upper


	Consider the sample triplet    0  1  2

   Comment July 13, 2007...

	This routine was not programmed as planned, or described.  Although
	it works with triplets, it only compares the first two samples.  If
	the difference between the 1st and 2nd is more than 1/256 the
	magnitude of the 1st, then the 2nd is replaced by the 1st.

	It would be improved by comparing the middle sample with the average
	of the other two, and by replacing the middle sample by the average
	if a spike is called.

*/

#include <stdio.h>


int hamali(unsigned long pp[][2],unsigned long *ps,int nch){
	int i,k;
	long f,g;

	k=0;
	for(i=0;i<nch;i++){
		// pressures
		if(pp[i][1]>pp[i][0]){	// figure out which is larger
			f=pp[i][1]-pp[i][0];
		}
		else{
			f=pp[i][0]-pp[i][1];
		}
		if (pp[i][0] != 0) {
			g=pp[i][0]/256L;
			if(f > g){
				ps[i]=pp[i][1];			// save for log
				pp[i][1]=pp[i][0];		// replace with first
				k++;
			}
		}
	}
	return k;
}
