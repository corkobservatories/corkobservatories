// Warning: the defines below must remain in sync with the
// array initialization in cvt_eerom.c
#define TX_WAKE_DELAY     			0
#define BASIC_SAMPLE_PERIOD 		1
#define THERMOMETER_WAIT			2
#define MODE_SELECTED_PERIOD		3
#define RTC_TIME_STAMP_DELAY		4
#define PPC_DATA_XMIT_TIME			5
#define NUM_CHANNELS				6
#define START_TIME					7
#define RTC_RATE_TWEAK				8
#define RTC_IDENT					9
#define RTC_META_DATA				10

#define SENSOR_WARMUP_TIME			0
#define TEMP_INTEGRATION_TIME		1
#define PRESSURE_INTEGRATION_TIME	2
#define MAX_SENSOR_STEADY_CURRENT	3
#define CALIBRATION_COEFFICIENTS	4
#define PPC_META_DATA				5

#define MT01_DATA					0
#define MT01_CNTRL					1
#define MT01_DWNLD					2
#define MT01_UART_LED				3
#define MT01_POWER_LED				4
#define MT01_ERROR_LED				5
#define MT01_WRITE_LED				6
#define MT01_PASS_LSTN				7
#define MT01_PASS_TALK				8
#define MT01_LSTN_MODE				9
#define MT01_LOW_VOLT				10
#define MT01_OK_VOLT				11


#define EEROM_LINE_LENGTH	 8
#define EEROM_LINES			16
#define EEROM_BUFSIZ		(EEROM_LINES * EEROM_LINE_LENGTH)

struct param_def {
	const char *name;
	char option;
	int offset;
	int len;
	int changeable;
	const char *(*cvtToUserReadable) (const unsigned char *eerom, int offset, int len);
	const unsigned char *(*cvtFromUserReadable) (const char *str, int len);
	int (*updateMT01Value) (char option, const char *str);
	const char *comment;
	const char *userval;
	const char *currval;
	const char *eeromval;
	const char *error;
};
