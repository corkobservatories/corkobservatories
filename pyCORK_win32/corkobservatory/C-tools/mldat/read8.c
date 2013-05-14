/* read8.c reads one byte, and pushes it into the LSB of x */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

extern FILE *pR,*pE,*pW,*pF, *pL;
extern void closeallf(void);

unsigned long read8(unsigned long x){
	unsigned char y;

   if((fread(&y, sizeof(y), 1, pR) != 1) || feof(pR)){
		closeallf();
		exit(0);
	}
   x=x<<8;
	x=x+y;
	return x;
}


