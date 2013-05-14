#ifndef _USERMENU_H
#define _USERMENU_H
//WARNING if changing things in here, change things in usermenu.c as well
#define HELP		'H'		//print usermenu
#define QCK_STATUS	'I' 	//voltage and percentage of memory used (I'm awake). Expected download time at default baud rate of ___.
#define FULL_STATUS	'F' 	//instrument ID, time remaining until memory full at sample rate ____ and at 1Hz if 'C' mode, clock offset. 
#define GET_META	'M' 	//copy metadata to file
#define DOWNLOAD	'D' 	//download data
#define MEM_CLEAR	'C' 	//clear memory
#define SET_TIME	'Y' 	//synchronize clock
#define FORM_DATA	'U' 	//print formatted datastrings. If bogus sitenum, print ascii datastrings
#define PAR_DNLOAD	'G' 	//copy EEROM and MT01 default settings to file
#define PAR_UPLOAD	'W' 	//load parameters from file into EEROM and MT01 default settings
#define DEPLOY 		'Z'	//deploy instrument
#define QUIT		'Q'  //quit
#define MLTERMLIN	'P' 	//WARNING: access to original MLTERM program for greater instrument handling. 
#define RECOVER		'R' 	//WARNING: force unstick MT01 from interface
#endif


