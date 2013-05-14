#include <stdio.h>
#include <unistd.h>

#include "mltermfuncs.h"

extern int kfd;

int
getch() {
	int rtn;
	unsigned char cc;

	rtn = read (kfd, &cc, 1);
	if (rtn != -1)
		rtn = (int) (unsigned int) cc;
	return rtn;
}

void WriteChar(int fd,char cc){
	write(fd, &cc, 1);
}

