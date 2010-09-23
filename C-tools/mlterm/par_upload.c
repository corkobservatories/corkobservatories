#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>

#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "usermenu.h"

static void paramQuit(int val, int high);

extern int kfd;

int par_upload(int high, FILE *inParamFile, FILE *outParamFile) { //const char *errfile) {
	//FILE *outParamFile;
	char infilename[MAXPATHLEN];
	char outfilename[MAXPATHLEN];
	int numerrs;
	int rtn;
	int write_mt01_rtn;
	int write_eerom_rtn;

	if (high) {
		if (TalkLstn (0)) { 
			Debug("Failed to set up for EEROM write\n");
			log_printf(LOG_SCREEN_MIN,"Failed to set up for EEROM write\n");
			TalkLstnRestore();
			return 1;
		}
		raw(kfd,0);  //raw so that an ESC char is registered (no <enter> required)
	}
	//parse_all_eeroms() doen't include MT01;
	if (parse_all_eeroms()) {
		paramQuit(1,high);
		return 1;
	}
	
	if (parse_mt01()) {
		paramQuit(1,high);
		Debug("Failed to parse MT01 menu.\n");
		return 1;
	}
	clear_uservals();  //not sure if this is necessary, but it makes certain any prior files loaded for updating that didn't finish uploading to EEROM won't have effect

	*infilename = '\0';
	if (!inParamFile) {
		restore_tty(kfd);
		inParamFile = opnparamfile("r", infilename, sizeof (infilename)); 
		raw(kfd,0);
	}
	if (inParamFile) {
/**
Should be checking return from parse_all_in_params:
 * return 0: success
 *        2: parse error
 *        3: immutable field
 *        4: system error
**/
		if ((rtn = parse_all_in_params(inParamFile)) == 0) //this includes MT01
			Debug("supposedly parsed input param file\n");
		else if (rtn > 1) {
			log_printf(LOG_SCREEN_MIN,"%s\n", get_last_parse_error());
			Debug("%s\n",get_last_parse_error());
			paramQuit(1,high);
			return 1;
		}
		fclose(inParamFile);
	}
	else {
		paramQuit(-1,high);
		return -1;
	}
	validate();
	if ((numerrs = check_param_errors()) >= 0) {  //warnings (giving positive numerrors) are preexisting in EEROM and don't inhibit changes to other values in EEROM
		log_printf(LOG_SCREEN_MIN, "Writing changes to eerom. Please be patient ...\n\n");
		slowtalk((unsigned char *) "Q"); //ensures we are not still in menu when !Q is sent 
		//write_all_eeroms doesn't include write_mt01
		if ((write_eerom_rtn = write_all_eeroms()) < 0) {
			log_printf(LOG_SCREEN_MIN, "User cancelled or problem writing to EEROM. Try again.\n");
			Debug("problem writing to EEROM\n");
			paramQuit(-1,high);
			return -1;
		} else if (write_eerom_rtn == 1) 
			log_printf(LOG_SCREEN_MIN, "No changes to EEROM.\n"); 
		else {
			if (high)
				log_printf(LOG_SCREEN_MIN, "Changes to EEROM completed successfully. Use menu option %c, %c or %c to verify.\n\n", PAR_DNLOAD, FULL_STATUS, GET_META);
			else	
				log_printf(LOG_SCREEN_MIN, "Changes to EEROM completed successfully. Get parameters or naked eerom to verify.\n\n");
		}
		invalidate_mt01_cache();
		parse_mt01(); 	//this will ensure up to date values in param_def struct (different than MenuInfo struct!!)
						// and that we are at menu prompt
		if ((write_mt01_rtn = write_mt01(get_mt01_param_def())) < 0) {
			log_printf(LOG_SCREEN_MIN, "problem writing MT01 defaults, check debug file to determine where\n");
			Debug("problem writing MT01 defaults\n");
		} 
		else if (write_mt01_rtn == 1)
			log_printf(LOG_SCREEN_MIN, "No changes to MT01.\n"); 
		else
			log_printf(LOG_SCREEN_MIN, "Changes to MT01 completed successfully. Use menu option %c to verify\n", QCK_STATUS); 
	}
	else {
		log_printf(LOG_SCREEN_MIN, "Error(s) with values in parameter input file. No changes to eerom made\n");
	}	
	if (numerrs) {
		log_printf(LOG_SCREEN_MIN, "\nChecking for parameter errors and warnings to write to error file.\n"); 
		restore_tty(kfd); //need to restore before filename entry
/*
		if (errfile) {
			outParamFile = fopen (errfile, "w");
			if (!outParamFile)
				perror (errfile);
		} else
			outParamFile = NULL;
*/
		// If we don't have an error file, try to make one from input file name
		if (!outParamFile && *infilename) {
			char *dot;
			char *slash;
			int ofd;

			strcpy (outfilename, infilename);
			if ((dot = strrchr (outfilename, '.')) != NULL) {
				slash = strrchr (outfilename, '/');
				if (!slash || dot > slash)
					*dot = '\0';
			}
			strcat (outfilename, ".perr");
			ofd = open(outfilename, O_WRONLY | O_CREAT |O_EXCL, 0644);
			if (ofd > 0) {
				close(ofd);
				outParamFile = fopen (outfilename, "w");
			}else if (ofd < 0) {
				perror(outfilename);
			}
		}
		if (!outParamFile)
			outParamFile = opnparamfile("w", outfilename, sizeof (outfilename));
		if (outParamFile) {
			Debug("Entering write_param_errors\n");
			raw(kfd,0);  //need to go back into raw here to ESC from probe_ppcs called from get_ppc_list from write_param_errors
			rtn = write_param_errors(outParamFile);
			restore_tty(kfd);
			if (rtn)
				Debug("Error writing parameter errors: errno = %d (%s)\n", errno, strerror (errno));
			else {
				Debug("Wrote parameter errors to output file\n");
				if (numerrs>0) log_printf(LOG_SCREEN_MIN, "\n Wrote %d warnings", numerrs);
				else log_printf(LOG_SCREEN_MIN, "Wrote %d errors", -numerrs);
				if (*outfilename) log_printf(LOG_SCREEN_MIN, " to %s\n", outfilename);
				else log_printf(LOG_SCREEN_MIN, " to designated file.\n");
			}
			fclose(outParamFile);
			Debug("Closed parameter file created\n");
		} else
			log_printf(LOG_SCREEN_MIN, "\nErrors not saved to file.\n");

		if (!high)
			raw(kfd,0);
	}
	paramQuit(write_mt01_rtn != 0 || numerrs < 0, high); //only restore talk listen if it hasn't been done already through write_mt01 func
	return 0;
}

static void
paramQuit(int val, int high) {
	if (high) {
		restore_tty(kfd);
		invalidate_mt01_cache ();   // force ^S to return to MT01 menu, and changes may have been made in write_MT01
		if (val) {
			TalkLstnRestore ();
		}
	} else {
		slowtalk((unsigned char *) "Q");
		Qtalk((unsigned char *) "!Q\r");
		slowtalk((unsigned char *) "!!\r");
	}
}
	



