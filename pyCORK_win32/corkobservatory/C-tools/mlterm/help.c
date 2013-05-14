/* help.c */

#include <stdio.h>

#include "mltermfuncs.h"

void 
help(void) {
//if changes are made here, you must also change isspecialkey.c	
	log_printf(LOG_SCREEN_MIN, "\nMT01 HOTKEYS (for use outside of MT01 menu prompt when MT01 awake)");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-s  MT01 menu");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-k  force recover MT01 from unknown state to menu (2009 and later versions only");
	//log_printf(LOG_SCREEN_MIN, "\n  ctrl-x  cancel download in progress");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-l  start MT01 logging");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-r  stop MT01 logging\n");
	
	log_printf(LOG_SCREEN_MIN, "\nMLTERM HOTKEYS");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-w  Wake MT01 logger");
	log_printf(LOG_SCREEN_MIN, "\n  H       Help for MLTERM(this menu)");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-e  Edit and <Enter> one EEROM line");
	log_printf(LOG_SCREEN_MIN, "\n  B       Show current MLTERM baud rate");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-b  Change MLTERM baud rate");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-d  Start MLTERM download at MT01 'Awaiting Connection' prompt");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-a  Get clock offset and format ascii time strings");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-u  Format data sentence into desired units");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-y  Set instrument clock time");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-z  Pause screen (display of incoming bytes)");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-g  Get parameters and save to file");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-o  Overwrite parameters with those loaded from file");
	log_printf(LOG_SCREEN_MIN, "\n  ctrl-n  Save naked EEROM to file");
	log_printf(LOG_SCREEN_MIN, "\n  ESC     Exit process ---> program ---> quit\n");
	
	fflush(stdout);
}
