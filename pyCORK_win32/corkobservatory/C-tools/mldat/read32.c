/* read32.c reads four bytes, swaps them, and returns a long integer */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

extern FILE *pR,*pW,*pF,*pE, *pL;
extern void closeallf(void);

unsigned long read32(void){
   unsigned long jj;
	unsigned char buff[4];

	if((fread(buff, sizeof(buff[0]), 4, pR) != 4) || feof(pR)) {
		closeallf();
		exit(0);
	}
   jj=buff[0]*256L;
   jj=(jj+buff[1])*256L;
   jj=(jj+buff[2])*256L;
   jj=(jj+buff[3]);
   return jj;
}

