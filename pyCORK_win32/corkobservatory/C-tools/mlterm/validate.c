//param def.userval and param def.eeromval are strings so they need to be compared with strings, or converted to integers or hex values before comparison.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mltermfuncs.h"
#include "param.h"
#include "ttyfuncdecs.h"

#define FieldVal(X)	((X).userval ? (X).userval : (X).eeromval)

static int ValidateBaud(const char *s);
static const char *BaudErrorMessage (void);
static int ValidateOnOff(const char *s);
static int ValidateBinAsc(const char *s);
static int ValidateLowVoltage(const char *s);
static int ValidateOKVoltage(const char *s);
static int ValidateMetaData (const char *s);
static void clear_errors(struct param_def *p);
static void append_error(struct param_def *p, const char *msg);

void
validate () {
	struct param_def *rtc_pd;
	struct param_def *p;
	const unsigned char *ppc_list;
	int ppc_ndx;
	int num_ppcs;
	int tx_wake_delay;
	int basic_sample_period;
	int thermometer_wait;
	int RTC_time_stamp_delay;
	int PPC_data_xmit_time;
	int num_channels;
	
	int sensor_warmup_time;
	int temp_integration_time;
	int pressure_integration_time;
	float max_sensor_steady_current;
	
	int td;
	float stime;
	int val;
	float fval;
	const unsigned char *bp;
	
	Debug("Entered validate\n");
	rtc_pd = get_rtc_param_def ();
	if (!rtc_pd)
		return;

	clear_errors (rtc_pd);

	sscanf (FieldVal (rtc_pd[TX_WAKE_DELAY]), "%d", &tx_wake_delay);
	sscanf (FieldVal (rtc_pd[BASIC_SAMPLE_PERIOD]), "%d", &basic_sample_period);
	sscanf (FieldVal (rtc_pd[THERMOMETER_WAIT]), "%d", &thermometer_wait);
	sscanf (FieldVal (rtc_pd[RTC_TIME_STAMP_DELAY]), "%d", &RTC_time_stamp_delay);
	sscanf (FieldVal (rtc_pd[PPC_DATA_XMIT_TIME]), "%d", &PPC_data_xmit_time);
	num_ppcs = 0;
	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return;

	for (ppc_ndx = 0; ppc_ndx < 8 && ppc_list[ppc_ndx]; ppc_ndx++) {
		p = get_ppc_param_def (ppc_ndx);
		if (!p)
			break;
		clear_errors (p);
		Debug("Got PPC %d param defs\n", ppc_list[ppc_ndx]);
		if (!ppc_ndx) {
			sscanf (FieldVal (p[SENSOR_WARMUP_TIME]), "%d", &sensor_warmup_time);
			if (sensor_warmup_time != 370)
				append_error (&p[SENSOR_WARMUP_TIME], "Only valid value is 370");
			sscanf (FieldVal (p[TEMP_INTEGRATION_TIME]), "%d", &temp_integration_time);
			if ((temp_integration_time != 0) && (temp_integration_time != 400))
				append_error (&p[TEMP_INTEGRATION_TIME], "Only valid values are 0 for paro-1 type gauge and 400 for paro-2 gauge");
			sscanf (FieldVal (p[PRESSURE_INTEGRATION_TIME]), "%d", &pressure_integration_time);
			if (pressure_integration_time != 800)
				append_error (&p[PRESSURE_INTEGRATION_TIME], "Only valid value is 800");
			sscanf (FieldVal (p[MAX_SENSOR_STEADY_CURRENT]), "%f", &max_sensor_steady_current);
			if ((max_sensor_steady_current <= 7.64) || (max_sensor_steady_current >= 10.15))
				append_error (&p[MAX_SENSOR_STEADY_CURRENT], "Valid values for max current range from  7.7 to  10.1 mA");
			if (ValidateMetaData (FieldVal (p[PPC_META_DATA])))
				append_error (&p[PPC_META_DATA], "PPC meta data string too long - 64 character max.");
		} else {
			sscanf (FieldVal (p[SENSOR_WARMUP_TIME]), "%d", &val);
			if (val != sensor_warmup_time) {
				append_error (&p[SENSOR_WARMUP_TIME], "Value differs from that of PPC 9");
				if (val != 370)
					append_error (&p[SENSOR_WARMUP_TIME], "Only valid value is 370");
			}
			sscanf (FieldVal (p[PRESSURE_INTEGRATION_TIME]), "%d", &val);
			if (val != pressure_integration_time) { 
				append_error (&p[PRESSURE_INTEGRATION_TIME], "Value differs from that of PPC 9");
				if (val != 800)
					append_error (&p[PRESSURE_INTEGRATION_TIME], "Only valid value is 800");
			}
			sscanf (FieldVal (p[TEMP_INTEGRATION_TIME]), "%d", &val);
			if (val != temp_integration_time) { 
				append_error (&p[TEMP_INTEGRATION_TIME], "Value differs from that of PPC 9");
				if ((val != 0) && (val != 400))
					append_error (&p[TEMP_INTEGRATION_TIME], "Only valid values are 0 for paro-1 type gauge and 400 for paro-2 gauge");
			}
			sscanf (FieldVal (p[MAX_SENSOR_STEADY_CURRENT]), "%f", &fval);
			if (fval != max_sensor_steady_current) {
				append_error (&p[MAX_SENSOR_STEADY_CURRENT], "Value differs from that of PPC 9");
				if ((fval <= 7.64) || (fval >= 10.15))
					append_error(&p[MAX_SENSOR_STEADY_CURRENT], "Valid values for max current are between 7.7 and 10.1 mA");
			}
			if (ValidateMetaData (FieldVal (p[PPC_META_DATA])))
				append_error (&p[PPC_META_DATA], "PPC meta data string too long - 64 character max.");
		}
	}
	Debug("Finished validating PPC params\n");
	num_ppcs = ppc_ndx;
 	if (strcasecmp (FieldVal (rtc_pd[NUM_CHANNELS]), "AUTO")) {
		sscanf (FieldVal (rtc_pd[NUM_CHANNELS]), "%d", &num_channels);
		if (num_ppcs < num_channels)
			append_error (&rtc_pd[NUM_CHANNELS], "Not enough PPCs are electrically connected for number of channels requested");
	} else 
		num_channels = num_ppcs;
	if (basic_sample_period <= 60) {//here we check that there is enough time for all tasks within sample period
		td=sensor_warmup_time+temp_integration_time+pressure_integration_time+tx_wake_delay+10; //+10ms for MTO1 to go to sleep.
		stime = td+num_channels*2.08*((temp_integration_time!=0)+(pressure_integration_time!=0))+1.2; 
		if(stime>(float)basic_sample_period*1000){
			char s[128];
			sprintf(s,"Sample period is too short for total of delays, integration and transmit times for num_channels = %.1fms",stime);
			append_error (&rtc_pd[BASIC_SAMPLE_PERIOD], s);
		}
		if (basic_sample_period == 1)
			append_error(&rtc_pd[BASIC_SAMPLE_PERIOD], "1Hz sampling must be designated in ModeSelectedPeriod setting.");
		if (60%basic_sample_period != 0) 
			append_error(&rtc_pd[BASIC_SAMPLE_PERIOD], "Valid entries are 2,3,4,5,6,10,15,20,30,60 for sub-minute sampling, \nand 60 times these values for sub-hourly sampling.");
		}
	else if (3600%basic_sample_period != 0) 
		append_error(&rtc_pd[BASIC_SAMPLE_PERIOD], "Valid entries are 2,3,4,5,6,10,15,20,30,60 for sub-minute sampling, \nand 60 times these values for sub-hourly sampling.");
	if (strcasecmp (FieldVal (rtc_pd[MODE_SELECTED_PERIOD]),"A")  && strcasecmp (FieldVal (rtc_pd[MODE_SELECTED_PERIOD]),"C") && strcasecmp (FieldVal (rtc_pd[MODE_SELECTED_PERIOD]),"1")) 
		append_error (&rtc_pd[MODE_SELECTED_PERIOD], "Not a valid mode entry. Enter 'A', 'C' or '1'");
	if (rtc_pd[START_TIME].userval && strcasecmp (rtc_pd[START_TIME].userval, "AUTO"))
		append_error (&rtc_pd[START_TIME], "AUTO for synchronization with computer clock is the only valid time entry.");
	if (rtc_pd[TX_WAKE_DELAY].cvtFromUserReadable (FieldVal (rtc_pd[TX_WAKE_DELAY]), rtc_pd[TX_WAKE_DELAY].len)[0] != 0x01) 
		append_error (&rtc_pd[TX_WAKE_DELAY], "10 milliseconds is the only valid entry");
	bp = rtc_pd[RTC_TIME_STAMP_DELAY].cvtFromUserReadable (FieldVal (rtc_pd[RTC_TIME_STAMP_DELAY]), rtc_pd[RTC_TIME_STAMP_DELAY].len);
	if (temp_integration_time == 0) {
		if (bp[0] != 0xF3 || (bp[1] != 0x81 && bp[1] != 0x80))
			append_error (&rtc_pd[RTC_TIME_STAMP_DELAY], "F380 or F381 are the only valid entries for xxx-1 type paroscientific gauges");
		if (rtc_pd[PPC_DATA_XMIT_TIME].cvtFromUserReadable (FieldVal (rtc_pd[PPC_DATA_XMIT_TIME]), rtc_pd[PPC_DATA_XMIT_TIME].len)[0] != 0xF9) 
			append_error (&rtc_pd[PPC_DATA_XMIT_TIME], "F9 is the only valid entry for pressure sensors without temperature period");
	}
	else {
		if ((bp[0] != 0xF0) || (bp[1] > 0x4D)) //|| (bp[1] != 0x4D && bp[1] != 0x00))
			append_error (&rtc_pd[RTC_TIME_STAMP_DELAY], "Entry must be between F000 and F04D for xxx-2 type paroscientific gauges");
		if (rtc_pd[PPC_DATA_XMIT_TIME].cvtFromUserReadable (FieldVal (rtc_pd[PPC_DATA_XMIT_TIME]), rtc_pd[PPC_DATA_XMIT_TIME].len)[0] != 0xF5)
			append_error (&rtc_pd[PPC_DATA_XMIT_TIME], "F5 is the only valid entry for pressure sensors with temperature period");
	}
	if (ValidateMetaData (FieldVal (rtc_pd[RTC_META_DATA])))
		append_error (&rtc_pd[RTC_META_DATA], "RTC meta data string too long - 64 character max.");
	Debug("Finished validating RTC params\n");	

	// Check MT01 values
	p = get_mt01_param_def ();
	if (!p)
		return;
	clear_errors (p);

	if (ValidateBaud (FieldVal (p[MT01_DATA])))
		append_error (&p[MT01_DATA], BaudErrorMessage ());
	if (ValidateBaud (FieldVal (p[MT01_CNTRL])))
		append_error (&p[MT01_CNTRL], BaudErrorMessage ());
	if (ValidateBaud (FieldVal (p[MT01_DWNLD])))
		append_error (&p[MT01_DWNLD], BaudErrorMessage ());

	if (ValidateOnOff (FieldVal (p[MT01_UART_LED])))
		append_error (&p[MT01_UART_LED], "Value must be either ON or OFF.");
	if (ValidateOnOff (FieldVal (p[MT01_POWER_LED])))
		append_error (&p[MT01_POWER_LED], "Value must be either ON or OFF.");
	if (ValidateOnOff (FieldVal (p[MT01_ERROR_LED])))
		append_error (&p[MT01_ERROR_LED], "Value must be either ON or OFF.");
	if (ValidateOnOff (FieldVal (p[MT01_WRITE_LED])))
		append_error (&p[MT01_WRITE_LED], "Value must be either ON or OFF.");
	if (ValidateOnOff (FieldVal (p[MT01_PASS_LSTN])))
		append_error (&p[MT01_PASS_LSTN], "Value must be either ON or OFF.");
	if (ValidateOnOff (FieldVal (p[MT01_PASS_TALK])))
		append_error (&p[MT01_PASS_TALK], "Value must be either ON or OFF.");

	if (ValidateLowVoltage (FieldVal (p[MT01_LOW_VOLT])))
		append_error (&p[MT01_LOW_VOLT], "Low Voltage should be 4960.");
	if (ValidateOKVoltage (FieldVal (p[MT01_OK_VOLT])))
		append_error (&p[MT01_OK_VOLT], "OK Voltage should be 6800.");

	if (FieldVal (p[MT01_LSTN_MODE]) && ValidateBinAsc (FieldVal (p[MT01_LSTN_MODE])))
		append_error (&p[MT01_PASS_TALK], "Value must be either BIN or ASC.");

	Debug("Finished validating MT01 params\n");	
}

static int
ValidateBaud (const char *s) {
	const speed_t *vp;
	speed_t br;
	long speed;
	char *cp;

	speed = strtol (s, &cp, 10);
	if (*cp || ((br = cvtSpeed (speed)) == 0))
		return 1;

	for (vp = getValidInstrumentBaudRates (); *vp; vp++) {
		if (br == *vp)
			return 0;
	}

	return 1;
}

static const char *
BaudErrorMessage (void) {
	static char msg[256];
	char rate[8];
	const speed_t *vp;

	strcpy (msg, "Invalid baud rate.  Valid rates are: ");

	for (vp = getValidInstrumentBaudRates (); *vp > 0; vp++) {
		sprintf (rate, vp[1] ? "%ld, " : "%ld", cvtBaud (*vp));
		strcat (msg, rate);
	}

	return msg;
}

static int
ValidateOnOff (const char *s) {
	if (!strcasecmp (s, "ON"))
		return 0;
	if (!strcasecmp (s, "OFF"))
		return 0;
	return 1;
}

static int
ValidateBinAsc (const char *s) {
	if (!strcasecmp (s, "BIN"))
		return 0;
	if (!strcasecmp (s, "ASC"))
		return 0;
	return 1;
}

static int
ValidateLowVoltage (const char *s) {
	long voltage;
	char *cp;

	voltage = strtol (s, &cp, 10);
	if (*cp || voltage !=  4960)    //  (voltage < 0) || (voltage > 99999))
		return 1;
	return 0;
}

static int
ValidateOKVoltage (const char *s) {
	long voltage;
	char *cp;

	voltage = strtol (s, &cp, 10);
	if (*cp || voltage !=  6800)    //  (voltage < 0) || (voltage > 99999))
		return 1;
	return 0;
}

static int
ValidateMetaData (const char *s) {
	int len;

	len = strlen (s);
	if (len <= 64)
		return 0;
	if ((len <= 66) && (s[0] == '"') && (s[len-1] == '"'))
		return 0;
	return 1;
}

static void
clear_errors (struct param_def *p) {
	for (; p && p->name; p++) {
		if (p->error) {
			free ((void *) p->error);
			p->error = NULL;
		}
	}
}


static void 
append_error (struct param_def *p, const char *msg) {
	if (p->error) {
		char *newerror = malloc (strlen (p->error) + strlen (msg) + 2);
		strcpy (newerror, p->error);
		strcat (newerror, "\n");
		strcat (newerror, msg);
		free ((void *) p->error);
		p->error = newerror; 
	} else
		p->error = strdup (msg);
}
