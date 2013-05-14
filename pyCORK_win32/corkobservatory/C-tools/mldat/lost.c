
/* lost() is called when the normal progression of time in the data
	file has been lost.  Starting with the word after the last good
	time, lost() looks through the data file byte by byte for a time
	value differing less than 86400s from the last good time.  If such
	a word is found, it then looks ahead for 44 bytes (9 pressure
	channels on version 2007) for another time.  In this case, it has
	to differ 60 sec or less from the time being confirmed.  The file
	is then repositioned at the 1st good time and lost() returns to
	the calling process.

	If the first new time is not confirmed, the file is re-positioned
	at the confirmation time which failed, and the search continues.

	If the end of file is reached, returned int = 0. */

#include <stdio.h>
#include <errno.h>

unsigned long read8(unsigned long);
unsigned long read32(void);

int lost(FILE *pR,long *pPos,unsigned long t1){

	int i;
	unsigned long t2,t3,d1,d2;
	long pos1;

	fseek(pR,*pPos,SEEK_SET);
	t2=read32();					// start with next 4 bytes

	do{
		t2=read8(t2);						// move along 1 byte
		d1=t2-t1;						// check difference from last good
		if( d1>0 && d1<86400 ){		// reasonable expection for a time
      		pos1 = ftell(pR);		// save position (+4 bytes)
			
         // if have possible time, try to confirm it
			t3=read32();				// start with next word
			for(i=0;i<44;i++){		// this covers 2007 max P sample
				t3=read8(t3);
				d2=t3-t2;
				if(d2<=1800){      // necessary for confirm
					pos1 = pos1 - 4;			// confirmed, so reposition file
					*pPos = pos1;
					return 0;
				}
			}
			// confirmation failed
			fseek(pR, pos1, SEEK_SET);		// restart after time which failed
		}
	}while(!feof(pR));
	return -1;
}


