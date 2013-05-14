#include <stdio.h>
#include <fcntl.h>  //don't think this is necessary
#include <string.h>

#include "ttyfuncdecs.h"


extern int kfd;

void
termln(char *in, int size){
	fgets(in, size, stdin);
	if(!strchr(in,'\n')){ //this is to grab any extra data in the stream beyond the size array 'in'
		char buf[1024];
		do {
			fgets(buf, sizeof(buf),stdin);
		} while (!strchr(buf,'\n'));
	}
}
