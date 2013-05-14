#include <sys/param.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"

extern int kfd;

int par_dnload(int high, FILE *outParamFile) {
	char name[MAXPATHLEN];

	if (high) {
		if (TalkLstn (0))
			return 1;
		else raw(kfd,0); //raw so that an ESC char is registered (no <enter> required)
	}
	if (parse_all_eeroms()) {
		Qtalk((unsigned char *) "!Q\r");
		slowtalk((unsigned char *) "!!\r");
		if (high) {
			invalidate_mt01_cache ();	// force ^S to return to MT01 menu
			TalkLstnRestore ();
			restore_tty(kfd);
		}
		return 1;
	}
	if (high) {
		invalidate_mt01_cache ();	// force ^S to return to MT01 menu
		Debug("attempting to restore Talk Lstn and Mode to original state\n");
		TalkLstnRestore (); //there doesn't appear to be any problem leaving 
							//Pass through mode w/o returning timestrings to spit.
		restore_tty(kfd);
	}
	if (parse_mt01()) {
		if (!high) {
			slowtalk((unsigned char *) "Q");
			Qtalk((unsigned char *) "!Q\r");
			slowtalk((unsigned char *) "!!\r");
		}
		Debug("Failed to parse MT01 menu.\n");
		return 1;
	}
	clear_uservals();   //don't want to write uservals to param file 
	validate();
	if (!outParamFile) {
		if (!high) restore_tty(kfd);
			outParamFile = opnparamfile("w", name, sizeof (name));
		if (!high) raw(kfd,0);
	}
	if (outParamFile) {
		write_all_params(outParamFile);
		Debug("wrote parameters to output file\n");
		log_printf(LOG_SCREEN_MIN, "Wrote parameters to designated file.\n");
		fclose(outParamFile);
		Debug("Closed parameter file created\n");
	}
	if (!high) {
		slowtalk((unsigned char *) "Q");
		Qtalk((unsigned char *) "!Q\r");
		slowtalk((unsigned char *) "!!\r");
	}
	if (!outParamFile) return 2;
	else return 0;
} 
