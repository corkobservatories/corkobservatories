#include <sys/param.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int kfd;

int get_meta(int high, FILE *outParamFile) {
	char name[MAXPATHLEN];

	if (!outParamFile) {
		if (!high) restore_tty(kfd);
			outParamFile = opnparamfile("w", name, sizeof (name));
	}
	raw(kfd,0); //raw so that an ESC char is registered (no <enter> required)
	if (outParamFile != NULL) {
		if (high) {
			if (TalkLstn(0))
				return 1;
		}
		if (dump_eeroms(outParamFile)) {
			if (high) {
				invalidate_mt01_cache(); //force ^s to return to MT01 menu
				TalkLstnRestore();
				restore_tty(kfd);
			} else {
				Qtalk((unsigned char *) "!Q\r");
				slowtalk((unsigned char *) "!!\r");
			}
			return 1;
		}
		Debug("wrote eerom to output file\n");
		log_printf(LOG_SCREEN_MIN, "Wrote eerom to designated file.\n");
		fflush(stdout);
		fclose(outParamFile);
		Debug("Closed eerom file created\n");
		if (high) {
			invalidate_mt01_cache();
			TalkLstnRestore();
			restore_tty(kfd);
		} else {
			Qtalk((unsigned char *) "!Q\r");
			slowtalk((unsigned char *) "!!\r");
		}
	} else {
		Debug("Failed to open output file for MetaData and parameter settings\n");
		log_printf(LOG_SCREEN_MIN,"Failed to open output file for MetaData and Calibration Coeffs\n");
		if (high) restore_tty(kfd);
		return 1;
	}	
	return 0;
} 
