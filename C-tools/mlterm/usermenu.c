/* usermenu.c */

#include <stdio.h>

#include "mltermfuncs.h"
#include "usermenu.h"

void
usermenu(void) {
	Debug("print user menu. \n\n");
	log_printf(LOG_SCREEN_MIN, "\nMLTERM menu options:\n\n");
	log_printf(LOG_SCREEN_MIN, "%c: print this Help menu\n", HELP);
	log_printf(LOG_SCREEN_MIN, "%c: Initial status (wakes instrument)\n", QCK_STATUS);
	log_printf(LOG_SCREEN_MIN, "%c: Full status (metadata, clock, etc.)\n", FULL_STATUS);
	log_printf(LOG_SCREEN_MIN, "%c: save Metadata and naked eerom to file\n", GET_META);
	log_printf(LOG_SCREEN_MIN, "%c: Download data\n", DOWNLOAD);
	log_printf(LOG_SCREEN_MIN, "%c: Clear memory\n", MEM_CLEAR);
	log_printf(LOG_SCREEN_MIN, "%c: sYnchronize instrument clock time\n", SET_TIME);
	log_printf(LOG_SCREEN_MIN, "%c: print data in engineering Units\n", FORM_DATA);
	log_printf(LOG_SCREEN_MIN, "%c: Get settings from instrument\n", PAR_DNLOAD);
	log_printf(LOG_SCREEN_MIN, "%c: Write settings to instrument\n", PAR_UPLOAD);
	log_printf(LOG_SCREEN_MIN, "%c: exit MLTERM program (and goodbye to instrument)\n\n", DEPLOY);
	log_printf(LOG_SCREEN_MIN, "Use with extra caution\n");
	log_printf(LOG_SCREEN_MIN, "%c: force Quit MLTERM program\n", QUIT);
	log_printf(LOG_SCREEN_MIN, "%c: low level (Pass-thru) mlterm (advanced users only)\n", MLTERMLIN);
	log_printf(LOG_SCREEN_MIN, "%c: Recover instrument from stuck interface\n", RECOVER);
	fflush(stdout);
}
