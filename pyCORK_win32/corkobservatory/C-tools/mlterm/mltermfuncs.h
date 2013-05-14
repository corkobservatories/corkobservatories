#ifndef _MLTERMFUNCS_H
#define _MLTERMFUNCS_H

#include <stdio.h>
#include <termios.h>
#include <sys/timeb.h>

#include "menu.h"

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define LF  0x0A
#define CR  0x0D
#define NAK 0x15
#define CAN 0x18
#define ESC 0x1B

#define LOG_NONE		-1		// Turn off all logging: file, screen, everything
#define LOG_FILE		0		// Only log to file
#define LOG_SCREEN_MIN	1		// Log a few messages to the screen, all to file
#define LOG_SCREEN_MED	2		// Log most messages to the screen, all to file
#define LOG_SCREEN_MAX	3		// Log all messages to the screen and file

#define max(x,y)        (((x) >= (y)) ? (x) : (y))
#define min(x,y)        (((x) <= (y)) ? (x) : (y))

#ifdef VPN
    // Define more relaxed timeouts for TCP/IP VPN connections
#   define fasttalk(x)		talk ((x), 0.525)
#   define slowtalk(x)		talk ((x), 0.750)
#   define Qtalk(x)		talk ((x), 1.5)
#else
    // Regular timeout settings for direct connections
#   define fasttalk(x)		talk ((x), 0.025)
#   define slowtalk(x)		talk ((x), 0.250)
#   define Qtalk(x)		talk ((x), 1.0)  //was able to reduce this time once I added an extra 
										  //1 second nap after the quit from menu in rtcppc.c
										  //I know that 0.5 still fails with the slower devices (RS422 to RS232 conversion) 
										  //0.75 failed with USB COM PRO
#endif

#define NumElm(x)		(sizeof (x) / sizeof (*(x)))

/* autodnload.c */
extern int autodnload(int ofd, unsigned long sectors);

/* clearserialbuf.c */
extern void clearserialbuf(void);

/* crc.c */
extern unsigned short cal_crc(unsigned char *ptr, int count);

/* cvt_eerom.c */
extern struct param_def *get_rtc_param_def(void);
extern struct param_def *get_ppc_param_def(int ppcndx);
extern struct param_def *get_mt01_param_def(void);
extern void clear_uservals(void);
extern int check_param_errors(void);
extern int parse_all_eeroms(void);
extern int parse_mt01(void);
extern int write_mt01 (struct param_def *pd);
extern int write_all_eeroms(void);

/* debug.c */
extern void setAlarmTimeout (int timeout);
extern int initDebug(const char *filename, const char *logfile);
extern int isDebug(void);
extern void Debug(const char *format, ...);
extern void DebugBuf(int len, const unsigned char *buf, const char *format, ...);
extern int readDebug(int fd, void *buf, size_t count, const char *name);
extern int writeDebug(int fd, const void *buf, size_t count, const char *name);
extern int writeDebugDelay(int fd, const void *buf, size_t count, const char *name, speed_t baud);
extern int ftime(struct timeb *tb);

/* dos2linux.c */
extern int getch(void);
extern void WriteChar(int fd, char cc);

/* download.c */
extern void cancel(int n);
extern void download(int ofd);

/* dump.c */
extern int dump_eeroms(FILE *fp);

/* formattime.c */
extern long long formattime(int detoffset);

/* formatdata.c */
extern void formatdata(char *sitenum, int high);
extern void formdataspit(unsigned char *dataspit, int bytecount, int data_format, char *sitenum, int high);
extern int initialize_vals(char *sitenum);
extern int getsn(int nTsensor, int nParo, char *sitenum);
extern int findTsensor(char *Tsensor, int nTsensor);
extern int findParo(long int Paro, int nParo, int j);
extern int platinum(void);
extern int therms(void);
extern int Parosci(void);
extern double calitemp(double tper, double *Tc, int i);
extern double caliplat(double temp, int i);
extern double calitherm(double temp, int i);
extern double calipress_2(double u, double pper, int i);
extern double calipress(double celcius, double pper, int i);
extern unsigned long bytes2long(const unsigned char *str, int len);
extern void spitforminfo(int dataformat);

/* get_meta.c */
extern int get_meta(int high, FILE *outParamFile); 

/* help.c */
extern void help(void);

/* hex2int.c */
extern unsigned char hex2int(char *str);

/* ishexstring.c */
extern int ishexstring(const char *s);
extern int isnhex(const char *s, int n);

/* isspecialkey.c */
extern int isspecialkey(unsigned char *cc, int len);

/* log_printf.c */
extern int log_open(const char *filename);
extern void log_close(void);
extern int set_log_level (int level);
extern int log_printf(int level, const char *format, ...);

/* lstn.c */
extern int lstn(unsigned char *b);

/* lstndouble.c */
extern int lstndouble(unsigned char *bold, unsigned char *bnew);

/* lstnMT01.c */
extern int lstnMT01(char *valid_str);
extern unsigned char *get_response_buf(void);

/* menu.c */
extern const MenuInfo *get_mt01_info(int request_menu, int force);
extern void invalidate_mt01_cache(void);
extern const MenuItem *getMenuItem(char opt, const MenuInfo *mi);

/*mltermlin.c */
extern void mltermlin(int pass_through_only);

/* mltermmenu.c */
extern int getint(int min, int max);
extern char *trim_user_ent(char *buf);

/* MT01funcs.c */
extern int DataBaud(char option, const char *rate);
extern int CntrlBaud(char option, const char *rate);
extern int DwnldBaud(char option, const char *rate);
extern int QuickSwitch(char option, const char *toggle);
extern int Pass(char option, const char *toggle);
extern int LowVolt(char option, const char *mvolt);
extern int OkVolt(char option, const char *mvolt);
extern int clear_mt01(void);
extern int deploy_mt01(void);
extern int save_mt01(void);
extern int ChangeBaud(const char *rate);
extern int RestoreBaud(void);
extern int StatusSwitch(const char *stat_str);
extern int TalkLstn (int lstn_only);
extern int TalkLstnRestore (void);
extern int recover(void);
extern int recoverOld(void);
extern int confirm(char *prompt);

/* opnfilna.c */
extern int opnfilna(void);

/* opnparamfile.c */
extern FILE *opnparamfile(const char *mode, char *name, int namesize);

/* par_dnload.c */
extern int par_dnload(int high, FILE *outParamFile); 

/* par_upload.c */
extern int par_upload(int high, FILE *inParamFile, FILE *errFile); 

/* parse_fileIO.c */
extern int parse_all_in_params(FILE *fp);
extern const char *get_last_parse_error(void);
extern int write_all_params(FILE *fp);
extern int write_param_errors(FILE *fp);

/* quit.c */
extern int quit(int high);

/* restore.c */
extern void restore(int status);

/* rtcppc.c */
extern unsigned char *get_ppc_list(void);
extern unsigned char *get_ppc_eerom(int ndx);
extern unsigned char *get_rtc_eerom(void);
extern void invalidate_eerom_cache(void);
extern void probe_ppcs(void);
extern void probe_rtc(void);

/* sbp.c */
extern int sbp(int ofd, int tot_blocks);

/* settime.c */
extern void settime(int high);

/* socket.c */
extern int netconnect(char *addr, int port);

/* status.c */
extern int qck_status(void);
extern int full_status(void);

/* stream.c */
extern int stream(int ofd);

/* strndup.c */
extern char *strndup (const char *s, size_t n);

/* talk.c */
extern void talk(unsigned char *cmd, float seconds);
extern void nap(float seconds);

/* termln.c */
extern void termln(char *in, int size);

/* usermenu.c */
extern void usermenu(void);

/* validate.c */
extern void validate(void);

/* wake.c */
extern int wake(int init_baud_only, char wakech);
extern int findBaud(int init_baud_only);

/* write_eerom_line.c */
extern int write_eerom_line(const unsigned char *line, int rtc_flag, int ppcnum, int addr);
extern int send_line(char *out, int flag);
extern int checkROM(char *in, int sbuflen);

/* xmocrc.c */
extern int xmocrc(int ofd);

/* signals.c */
extern int initAlarm (void);

#endif
