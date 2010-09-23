#include <stdio.h>

extern FILE *pR, *pW, *pE, *pF, *pL;

void closeallf(void)
{
	printf("\n");
	fclose(pR);
	fclose(pW);
	fclose(pE);
	fclose(pF);
	fclose(pL);
}


