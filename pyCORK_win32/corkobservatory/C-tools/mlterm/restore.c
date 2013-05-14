#include <stdio.h>
#include <stdlib.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int kfd;

void
restore(int status) {
	putchar ('\r');
	putchar ('\n');
	Debug("restore called with exit status = %d\n", status);
	final_restore_tty(kfd);
	exit(status);
}
