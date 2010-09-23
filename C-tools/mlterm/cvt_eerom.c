//cvt_eerom deals with parsing all instrument parameters (MT01, RTC, and PPCs)
//converting parameters to a useable value and filling the parameter structs
//with these values. It also has functions to deal with writing RTC and PPC
//EEROM to the instrument. MT01 params are written to the instrument from 
//here too (recently moved function write_mt01 from menu.c).  
//cvt_eerom also has various functions for returning necessary info
//to other functions, like the number of errors in the parameters structures,
//pointers to the param. structs. Also, the function to clear uservals from param structs. 

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mltermfuncs.h"
#include "param.h"

#define HexDigit2Ascii(X)	((X) <= 0x09 ? (X) + '0' : (X) - 0x0A + 'A')
#define Ascii2HexDigit(X)	((X) <= '9'  ? (X) - '0' : (X) + 0x0A - 'A')

static int write_eerom(const struct param_def *pd, const unsigned char *eerom, int rtc_flag, int ppcnum);
static void parse_eerom(struct param_def *pd, const unsigned char *eerom);
static const char *to_sample_conversion(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_sample_conversion(const char *str, int len);
static const char *to_hex_conversion(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_hex_conversion(const char *str, int len);
static const char *to_times10(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_times10(const char *str, int len);
static const char *to_mode_conversion(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_mode_conversion(const char *str, int len);
static const char *to_channel_conversion(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_channel_conversion(const char *str, int len);
static const char *to_time_conversion(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_time_conversion(const char *str, int len);
static const char *to_sensor_current(const unsigned char *eerom, int offset, int len);
static const unsigned char *from_sensor_current(const char *str, int len);
static const char *to_meta_conversion (const unsigned char *eerom, int offset, int len);
static const unsigned char *from_meta_conversion (const char *str, int len);

// Warning: the array initializations below must remain
// in sync with the defines in param.h
static struct param_def rtc_pd[] = { //see param.h to see what each item means
	{"TxWakeDelay",             0, 0x40,  1, 0, to_times10,            from_times10,            NULL, "MTO1Wake delay in milliseconds (fixed)."},
	{"SamplePeriod",            0, 0x41,  1, 1, to_sample_conversion,  from_sample_conversion,  NULL, "Sample period in seconds (user editable).\nSample period must evenly divide 60 (subminute) or 3600 (subhourly)."},
	{"ThermometerWait",         0, 0x42,  1, 0, to_times10,            from_times10,            NULL, "Delay until temperature measurement in milliseconds (fixed)."}, //Ali removed editability.
	{"ModeSelectedPeriod",      0, 0x43,  1, 1, to_mode_conversion,    from_mode_conversion,    NULL, "Mode selected sample period (user editable).\nEnter 'A' for autonomous, 'C' for cabled, '1' for 1 Hz."},
	{"RTCTimeStampDelay",       0, 0x44,  2, 1, to_hex_conversion,     from_hex_conversion,     NULL, "Delay until time stamp is sent to MT01 (edits not recommended!)."}, //"Should be greater than the sum of SensorWarmupTime, Pressure integration time, and PPCDataXmitTime*NumChannels."},
	{"PPCDataXmitTime",         0, 0x46,  1, 1, to_hex_conversion,     from_hex_conversion,     NULL, "Time to transmit 4 bytes of data from PPC (edits not recommended!)."},
	{"NumChannels",             0, 0x48,  8, 1, to_channel_conversion, from_channel_conversion, NULL, "Number of PPC channels (user editable).\nMaximum is 7."}, //8 bytes okay, but the next byte(0x56) must be 00.
																								//I won't want to print this param to file, I just want to check it, 
																								//and change it if it is non zero giving a warning.
	{"StartTime",               0, 0x70,  7, 0, to_time_conversion,    from_time_conversion,    NULL, "Start time (fixed). Use Synchronize Clock option in mlterm."},//Ali removed editability. "AUTO" userval doesn't work because then we have to use !S instead of ^c. Complicates things greatly
	{"RTCRateTweak",            0, 0x7C,  1, 0, to_hex_conversion,     from_hex_conversion,     NULL, "RTC Rate Tweak (REALLY fixed)."},
	{"RTCIdent",                0, 0x7E,  1, 0, to_hex_conversion,     from_hex_conversion,     NULL, "RTC Identifier (REALLY fixed)."},
	{"MetaData",				0, 0x00, 64, 1, to_meta_conversion,    from_meta_conversion,    NULL, "64bytes:1234567890123456789012345678901234567890123456789012345678901234"},
	{NULL,                      0, 0x00,  0, 0, NULL,                  NULL,                    NULL, NULL}
};

static const struct param_def ppc_pd[] = {
	{"SensorWarmupTime",        0, 0x78, 1,  1, to_times10,            from_times10,            NULL, "Paroscientific gauge warmup in milliseconds (edits not recommended)"},
	{"TempIntegrationTime",     0, 0x79, 1,  1, to_times10,            from_times10,            NULL, "Enter 0 ms for Paro-1 gauges with no temperature measurement (edits not recommended)"},
	{"PressureIntegrationTime", 0, 0x7A, 1,  1, to_times10,            from_times10,            NULL, "in milliseconds (edits not recommended)"},
	{"MaxSensorSteadyCurrent",  0, 0x7C, 1,  1, to_sensor_current,     from_sensor_current,     NULL, "in mA (edits not recommended)"},
	{"CalibrationCoefficients", 0, 0x70, 8,  0, to_hex_conversion,     from_hex_conversion,     NULL, "PPC Calibration Coefficients (REALLY fixed)."},
	{"MetaData",				0, 0x00, 64, 1, to_meta_conversion,    from_meta_conversion,    NULL, "64bytes:1234567890123456789012345678901234567890123456789012345678901234"},
	{NULL,                      0, 0x00, 0,  0, NULL,                  NULL,                    NULL, NULL}
};

static struct param_def mt01_pd[] = {
	{"Data",                  '1', 0x00, 0, 0, NULL,                  NULL,                    DataBaud,    "Data Baudrate (REALLY fixed)"},
	{"Cntrl",                 '2', 0x00, 0, 1, NULL,                  NULL,                    CntrlBaud,   "Control Baudrate"},
	{"Dwnld",                 '3', 0x00, 0, 1, NULL,                  NULL,                    DwnldBaud,   "Download Baudrate"},
	{"UARTLED",               '4', 0x00, 0, 1, NULL,                  NULL,                    QuickSwitch, "UART LED (OFF for deployment)"},
	{"PowerLED",              '5', 0x00, 0, 1, NULL,                  NULL,                    QuickSwitch, "Power LED (OFF for deployment)"},
	{"ErrorLED",              '6', 0x00, 0, 1, NULL,                  NULL,                    QuickSwitch, "Error LED (OFF for deployment)"},
	{"WriteLED",              '7', 0x00, 0, 1, NULL,                  NULL,                    QuickSwitch, "Write LED (OFF for deployment)"},
	{"PassLstn",              '8', 0x00, 0, 1, NULL,                  NULL,                    Pass,        "Pass Listen (ON to echo data over serial line)"},
	{"PassTalk",              '9', 0x00, 0, 1, NULL,                  NULL,                    Pass,        "Pass Talk (OFF for deployment!)"},
	{"LstnMode",              '0', 0x00, 0, 1, NULL,                  NULL,                    QuickSwitch, "Listen Mode (ASC or BIN)"},
	{"LowVolt",               'L', 0x00, 0, 0, NULL,                  NULL,                    LowVolt,     "Low Voltage (fixed)"},
	{"OkVolt",                'O', 0x00, 0, 0, NULL,                  NULL,                    OkVolt,      "OK Voltage (fixed)"},
	{NULL,                      0, 0x00, 0, 0, NULL,                  NULL,                    NULL,         NULL}
};


struct param_def *
get_rtc_param_def () {
	return rtc_pd;
}

struct param_def *
get_ppc_param_def (int ppcndx) {
	static struct param_def *ppc_pds[8];
	static const unsigned char *ppc_list;

	if (!ppc_list) {
		ppc_list = get_ppc_list ();
		if (!ppc_list)
			return NULL;
	}

	if (ppc_list[ppcndx] && !ppc_pds[ppcndx]) {
		ppc_pds[ppcndx] = malloc (sizeof (ppc_pd));
		memcpy (ppc_pds[ppcndx], ppc_pd, sizeof (ppc_pd));
	}

	return ppc_pds[ppcndx];
}

struct param_def *
get_mt01_param_def () {
	return mt01_pd;
}

// Clear uservals for RTC, PPC, and MT01
void
clear_uservals () {
	const unsigned char *ppc_list;
	struct param_def *p;
	int i;
	Debug("Clearing uservals\n");
	for (p = get_rtc_param_def (); p && p->name; p++) {
		if (p->userval)
			free ((void *) p->userval);
		p->userval = NULL;
	}

	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return;

	for (i = 0; (i < 8) && ppc_list[i]; i++) {
		for (p = get_ppc_param_def (i); p && p->name; p++) {
			if (p->userval)
				free ((void *) p->userval);
			p->userval = NULL;
		}
	}

	for (p = get_mt01_param_def (); p && p->name; p++) {
		if (p->userval)
			free ((void *) p->userval);
		p->userval = NULL;
	}
}

// Check for param errors for RTC, PPC, and MT01. Of course validate() must be 
// called before calls to check_param_errors for any errors to be generated.
int
check_param_errors () {
	const unsigned char *ppc_list;
	struct param_def *p;
	int warnings = 0;
	int errors = 0;
	int i;

	for (p = get_rtc_param_def (); p && p->name; p++) {
		if (p->error) {
			Debug ("check_param_errors: %s for RTC param %s\n", p->userval ? "error" : "warning", p->name);
			p->userval ? errors++ : warnings++;
		}
	}

	ppc_list = get_ppc_list ();
	if (ppc_list) {
		for (i = 0; (i < 8) && ppc_list[i]; i++) {
			for (p = get_ppc_param_def (i); p->name; p++) {
				if (p->error) {
					Debug ("check_param_errors: %s for PPC %d param %s\n", p->userval ? "error" : "warning", ppc_list[i], p->name);
					p->userval ? errors++ : warnings++;
				}
			}
		}
	}

	for (p = get_mt01_param_def (); p && p->name; p++) {
		if (p->error) {
			Debug ("check_param_errors: %s for MT01 param %s\n", p->userval ? "error" : "warning", p->name);
			p->userval ? errors++ : warnings++;
		}
	}

	Debug("check_param_errors: %d errors on user entered param vals\n",errors);
	Debug("check_param_errors: %d warnings on parameters not manipulated by user\n", warnings);
	return errors ? -errors : warnings;
}

// Parse current instrument values for RTC and PPC
int
parse_all_eeroms () {
	const unsigned char *ppc_list;
	int i;
	unsigned char *rtn;

	Debug("Try to get RTC EEROM\n");
	rtn = get_rtc_eerom();	
	if (rtn) {
		parse_eerom (get_rtc_param_def (), rtn); 
		Debug("got RTC EEROM, now try for PPC EEROM\n");
	} else
		return 1;

	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return 1;
	Debug("got PPC list\n");

	for (i = 0; (i < 8) && ppc_list[i]; i++) {
		Debug("parse PPC %d\n",ppc_list[i]);
		parse_eerom (get_ppc_param_def (i), get_ppc_eerom (i));
	}
	Debug("finished parsing PPC EEROM\n");

	return 0;
}

// Parse instrument values for MT01, 
// and fills in the param_def struct for the MT01
int
parse_mt01 () {
	const MenuInfo *mi;
	const MenuItem *item;
	struct param_def *mt01_pd;
	struct param_def *p;

	Debug("updating MT01 param def struct\n"); 
	if ((mt01_pd = get_mt01_param_def ()) == NULL)
		return 1;

	if ((mi = get_mt01_info (1, 1)) == NULL)  //changed second flag to 1 to force update of mt01 info before parsing
		return 1;

	for (p = mt01_pd; p->name; p++) {
		if ((item = getMenuItem (p->option, mi)) != NULL) {
			if (p->eeromval)
				free ((void *) p->eeromval);
			p->eeromval = strdup (item->dflt);
			if (p->currval)
				free ((void *) p->currval);
			p->currval  = strdup (item->curr);
		}
	}

	return 0;
}


//CAUTION. MUST CALL parse_mt01 IMMEDIATELY PRIOR TO CALLS TO write_mt01
int
write_mt01 (struct param_def *pd) {
//write_mt01 returns 0 if successful changing params, 1 if no params changed, -1 if unsuccessful
	struct param_def *p;
	int changed = 0;
	int rtn = 0;
	
	Debug("write_mt01\n");
	//writeDebug(ifd, (unsigned char *) "\x13",1,"instrument");
	for (p = pd; p->name && !rtn; p++) {
		Debug("write_mt01: %s\n", p->name);
		// Make sure this version of the instrument supports this parameter
		if (p->eeromval) {
			if (p->userval && strcasecmp (p->userval, p->eeromval)) {
				if (strcasecmp(p->userval, p->currval)) { 
					Debug("%s overwrite currval %s with userval %s\n",p->name,p->currval,p->userval);
					log_printf(LOG_SCREEN_MIN, "changing MT01 %s to: %s\n",p->name,p->currval,p->userval);
					rtn = p->updateMT01Value (p->option, p->userval);
					if (p->currval)
						free ((void *) p->currval);
					p->currval = strdup (p->userval);
				}
				changed = 1;
			}
		}
	}
	Debug("check changed\n");
	if (changed) {
		for (p = pd; p->name && !rtn; p++) {
			if (p->eeromval) {
				if (p->userval && strcasecmp (p->userval, p->currval)) { // note above for checking against eeromval, this checks against currval
					Debug("%s overwrite currval %s with userval %s\n",p->name,p->currval,p->userval);
					rtn = p->updateMT01Value (p->option, p->userval);
				} else if (!p->userval && strcasecmp (p->currval, p->eeromval)) {
					Debug("%s overwrite currval %s with eeromval %s\n",p->name,p->currval,p->eeromval);
					rtn = p->updateMT01Value (p->option, p->eeromval);
				}
			}
		}
		Debug("entering invalidate_mt01_cache()\n");
		invalidate_mt01_cache ();
		if (!rtn)
			rtn = save_mt01 ();
	} else {
		Debug("Nothing has changed\n");
		rtn = 1;
	}
	return rtn;
}

// Write new RTC and PPC values to the instrument
int
write_all_eeroms () {
	const unsigned char *ppc_list;
	int changed_rtc = 0, changed_ppc = 0;
	int rtn;
	int i;

	Debug("Entering routine to write all EEROMs\n"); 

	if ((rtn = write_eerom (get_rtc_param_def (), get_rtc_eerom (), 1, 0)) < 0) {
		invalidate_eerom_cache ();
		return rtn;
	}
	else if (rtn == 0) {
		changed_rtc = 1;
		log_printf(LOG_SCREEN_MIN, "\n");
	}
	ppc_list = get_ppc_list ();
	if (ppc_list) {
		for (i = 0; (i < 8) && ppc_list[i]; i++) {  //FIX really this should only go up to num_channels, 
													//not through all entries in ppc_list. for now it is okay, it doesn't hurt anything.
			if ((rtn = write_eerom (get_ppc_param_def (i), get_ppc_eerom (i), 0, ppc_list[i])) < 0)
				break;
			else if (rtn == 0) {
				changed_ppc = 1;
				log_printf(LOG_SCREEN_MIN, "\n");
			}
		}
	} else
		rtn = -1;

	if (changed_rtc || changed_ppc || (rtn == -1)) {
		invalidate_eerom_cache ();
		if (rtn > 0)
			rtn = 0;  //this will indicate change on return
	}
	return rtn;
}

//checks whether eerom val has been changed if so, writes the new line containing EEROM val to EEROM, returns 0 for success, 1 for nothing written, <0 for error.
static int
write_eerom (const struct param_def *pd, const unsigned char *eerom, int rtc_flag, int ppcnum) {
	const struct param_def *p;
	const unsigned char *new;
	unsigned char new_eerom[EEROM_LINES * EEROM_LINE_LENGTH];
	int line_num;
	int change_in_eerom = 0;
	int rtn;

	Debug("write_eerom: rtc_flag=%d  ppcnum=%d\n", rtc_flag, ppcnum);
	if (!pd || !eerom)
		return -1;

	// Make a copy of eerom for updates
	memcpy (new_eerom, eerom, sizeof (new_eerom));

	// Now update that copy with changes from param file
	for (p = pd; p->name; p++) {
		if (p->userval) {
			new = p->cvtFromUserReadable (p->userval, p->len);
			if (new) {
				if (memcmp (&eerom[p->offset], new, p->len)) {
					memcpy (&new_eerom[p->offset], new, p->len);
					Debug ("%s: userval=%s  offset=%X  len=%d\n", p->name, p->userval, p->offset, p->len);
					DebugBuf (p->len, &eerom[p->offset], "%s: old:\n", p->name);
					DebugBuf (p->len, &new_eerom[p->offset], "%s: new:\n", p->name);
					if (rtc_flag)
						log_printf(LOG_SCREEN_MIN,"changing RTC %s to: %s\n", p->name, p->userval);
					else
						log_printf(LOG_SCREEN_MIN,"changing PPC%d %s to: %s\n", ppcnum, p->name, p->userval);
				}
				free ((unsigned char *) new);
			}
		}
	}

	// And write out the changed lines to eerom
	for (line_num = 0; line_num < EEROM_LINES; line_num++) {
		if (memcmp (&eerom[EEROM_LINE_LENGTH*line_num], &new_eerom[EEROM_LINE_LENGTH*line_num], EEROM_LINE_LENGTH)) {
			if (rtc_flag)
				DebugBuf(EEROM_LINE_LENGTH, &new_eerom[EEROM_LINE_LENGTH*line_num], "RTC line %X\n", line_num);
			else
				DebugBuf(EEROM_LINE_LENGTH, &new_eerom[EEROM_LINE_LENGTH*line_num], "PPC %d line %X\n", ppcnum, line_num);
			if ((rtn = write_eerom_line (&new_eerom[EEROM_LINE_LENGTH*line_num], rtc_flag, ppcnum, line_num)) != 0) {
				return rtn;
			}
			change_in_eerom = 1;
		}
	}

	return change_in_eerom ? 0 : 1;
}

// Parses binary eerom (RTC or PPC) values
static void
parse_eerom (struct param_def *pd, const unsigned char *eerom) {
	struct param_def *p;

	if (!pd || !eerom)
		return;

	for (p = pd; p->name; p++) {
		if (p->eeromval)
			free ((void *) p->eeromval);
		p->eeromval = p->cvtToUserReadable (eerom, p->offset, p->len);
	}
}

static const char *
to_sample_conversion (const unsigned char *eerom, int offset, int len) {
	unsigned long val;
	char buf[32];

	if (!eerom)
		return NULL;

	eerom += offset;
	if (eerom[0] & 0x80)
		val = 60 * (eerom[0] & 0x7F);
	else
		val = eerom[0];

	sprintf (buf, "%ld", val);
	return strdup (buf);
}

static const unsigned char *
from_sample_conversion (const char *str, int len) {
	unsigned long val = strtoul (str, NULL, 10);
	unsigned char *bp;

	bp = malloc (len);

	if (val > 60)
		bp[0] = 0x80 | (val / 60);
	else
		bp[0] = val;

	return bp;
}

static const char *
to_hex_conversion (const unsigned char *eerom, int offset, int len) {
	char *bp;
	int i;

	if (!eerom)
		return NULL;

	bp = malloc (2 * len + 1);

	eerom += offset;
	for (i = 0; i < len; i++) {
		bp[2*i]   = HexDigit2Ascii (eerom[i] >> 4);
		bp[2*i+1] = HexDigit2Ascii (eerom[i] & 0x0F);
	}
	bp[2*len] = '\0';

	return bp;
}

static const unsigned char *
from_hex_conversion (const char *str, int len) {
	unsigned char *bp;
	int i;

	bp = malloc (len);

	for (i = 0; i < 2*len; i += 2)
		bp[i/2] = (Ascii2HexDigit (str[i]) << 4) | Ascii2HexDigit (str[i+1]);

	return bp;
}

static const char *
to_times10 (const unsigned char *eerom, int offset, int len) {
	unsigned long val;
	char buf[32];
	int i;

	if (!eerom)
		return NULL;

	eerom += offset;
	for (i = 0, val = 0; i < len; i++)
		val = 0x100 * val + eerom[i];

	sprintf (buf, "%ld", 10 * val);
	return strdup (buf);
}

static const unsigned char *
from_times10 (const char *str, int len) {
	unsigned long val = strtoul (str, NULL, 10) / 10;
	unsigned char *bp;
	int i;

	bp = malloc (len);

	for (i = 0; i < len; i++) {
		bp[len-i-1] = val & 0xFF;
		val >>= 8;
	}

	return bp;
}

static const char *
to_mode_conversion (const unsigned char *eerom, int offset, int len) {
	char buf[2];

	if (!eerom)
		return NULL;

	eerom += offset;
	switch (eerom[0]) {
		case 0x00:  buf[0] = '1';  break;
		case 0x8D:  buf[0] = 'C';  break;
		case 0xFF:  buf[0] = 'A';  break;
		default:    buf[0] = '?';  break;
	}
	buf[1] = '\0';

	return strdup (buf);
}

static const unsigned char *
from_mode_conversion (const char *str, int len) {
	unsigned char *bp;

	bp = malloc (len);

	switch (toupper (str[0])) {
		case '1':  bp[0] = 0x00;  break;
		case 'C':  bp[0] = 0x8D;  break;
		case 'A':  bp[0] = 0xFF;  break;
		default:   bp[0] = 0xFF;  break; //why is there a default. the default should be whatever the parameter was originally.
	}

	return bp;
}

static const char *
to_channel_conversion (const unsigned char *eerom, int offset, int len) {
	unsigned long val;
	char buf[32];
	int i;

	if (!eerom)
		return NULL;

	eerom += offset;
	for (i = 0, val = 0; i < len; i++) {
		if (eerom[i])
			val++;
		else
			break; //any channels entered after a 00 will not be counted. GOOD. 
	}

	sprintf (buf, "%ld", val); //why long?
	return strdup (buf);
}

static const unsigned char *
from_channel_conversion (const char *str, int len) {
	const unsigned char *ppc_list;
	unsigned char *bp;
	int num_channels;
	
	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return NULL;

	bp = malloc (len);
	memcpy (bp, ppc_list, len);

	if (strcasecmp (str, "AUTO")) {
		num_channels = atoi (str);
		if (num_channels < len)
			memset (&bp[num_channels], 0, len - num_channels);
	}

	return bp;
}

static const char *
to_time_conversion (const unsigned char *eerom, int offset, int len) {
	char buf[32];

	if (!eerom)
		return NULL;

	eerom += offset;
	sprintf (buf, "%u%02u/%02u/%02u %02u:%02u:%02u", (unsigned int) eerom[0], (unsigned int) eerom[1], (unsigned int) eerom[2], (unsigned int) eerom[3], (unsigned int) eerom[4], (unsigned int) eerom[5], (unsigned int) eerom[6]);
	return strdup (buf);
}

static const unsigned char *							
from_time_conversion (const char *str, int len) {	
// this doesn't do anything with *str. I think that is okay, 
// because I only want to reset time to now. 
// however, system clock's time is likely wrong, so this 
// param shouldn't be changeable via the read in file.
// furthermore, if we change this time, a different save 
// EEROM command needs to be sent to the instrument.
// solution: just make this parameter uneditable.
	unsigned char *bp;
	struct tm *tm;
	time_t t;
	
	bp = malloc (len);
	memset (bp, 0, len);

	time (&t);
	t++;// is this just a 1 second buffer? I'll likely need much more
	tm = gmtime (&t);

	bp[0] = (tm->tm_year + 1900) / 100;
	bp[1] = tm->tm_year % 100;
	bp[2] = tm->tm_mon + 1;
	bp[3] = tm->tm_mday;
	bp[4] = tm->tm_hour;
	bp[5] = tm->tm_min;
	bp[6] = tm->tm_sec;

	return bp;
}

//static const char *
//to_rtc_delay_conversion (const unsigned char *eerom, int offset, int len) {
//	unsigned long val;
//	char buf[32];
//
//	val = (0x10000 - (0x100 * eerom[offset] + eerom[offset+1])) * 1000.0 / 2048.0;
//	sprintf (buf, "%ld", val);
//	return strdup (buf);
//}

//static const unsigned char *
//from_rtc_delay_conversion (const char *str, int len) {
//	unsigned long val;
//	unsigned char *bp;
//
//	val = 0x10000 - (unsigned int) (atoi (str) * 2048.0 / 1000.0 - 0.5);
//	bp = malloc (len);
//	bp[0] = (val & 0xFF00) >> 0x08;
//	bp[1] = val & 0xFF;
//
//	return bp;
//}

//static const char *
//to_ppc_xmit_conversion (const unsigned char *eerom, int offset, int len) {
//	float val;
//	char buf[32];
//
//	val = ((0x100 - eerom[offset]) * 1000.0 / 2048.0) - 1.2;
//	sprintf (buf, "%.2f", val);
//	return strdup (buf);
//}

//static const unsigned char *
//from_ppc_xmit_conversion (const char *str, int len) {
//	unsigned long val;
//	unsigned char *bp;
//
//	val = 0x100 - (unsigned int) ((atof (str) + 1.2) * 2048.0 / 1000.0 - 0.5);
//	bp = malloc (len);
//	bp[0] = val & 0x00FF;
//
//	return bp;
//}

static const char *
to_sensor_current (const unsigned char *eerom, int offset, int len) {
	float val;
	char buf[32];

	if (!eerom)
		return NULL;

	eerom += offset;
	val = ((eerom[0]) * 33.0 / 256.0);
	sprintf (buf, "%.1f", val);
	return strdup (buf);
}

static const unsigned char *
from_sensor_current (const char *str, int len) {
	double val;
	unsigned char *bp;

	val = atof (str) * 256.0 / 33.0;
	bp = malloc (len);
	// Add 0.5 so we round, rather than truncate
	bp[0] = ((unsigned int) (val + 0.5)) & 0x00FF;

	return bp;
}

static const char *
to_meta_conversion (const unsigned char *eerom, int offset, int len) {
	char buf[67];
	char *bp;

	if (!eerom)
		return NULL;

	eerom += offset;
	for (bp = buf; len && *eerom; eerom++, bp++, len--)
		*bp = isprint (*eerom) ? *eerom : '.';
	*bp = '\0';

	if (isspace (buf[0]) || (bp == buf) || isspace (bp[-1])) {
		memmove (&buf[1], buf, bp - buf);
		buf[0] = '"';
		bp[1] = '"';
		bp[2] = '\0';
	}

	return strdup (buf);
}

static const unsigned char *
from_meta_conversion (const char *str, int len) {
	int slen;
	unsigned char *bp;

	bp = malloc (len);
	memset (bp, 0, len);
	slen = strlen (str);
	if ((str[0] == '"') && (str[slen - 1] == '"') && (slen >= 2))
		strncpy ((char *) bp, &str[1], min (len, slen - 2));
	else
		strncpy ((char *) bp, str, len);

	return bp;
}
