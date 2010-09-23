#ifndef _TTYFUNCDECS_H
#define _TTYFUNCDECS_H

#include <termios.h>
#include <unistd.h>

#define SECTOR_BUFFER_SIZE      512

#define max(x,y)        (((x) >= (y)) ? (x) : (y))

extern void setNetworkConnection (int flag);
extern int isNetworkConnection (void);
extern void save_tty(int fd);
extern void restore_tty(int fd);
extern void final_restore_tty(int fd);
extern int raw(int fd, speed_t speed);
extern int set_speed(int fd, speed_t speed);
extern int is230Valid (void);
extern const speed_t *getValidInstrumentBaudRates(void);
extern void opncom(int ifd);
extern const char *match_baud(int ifd, char *sBuf);
extern speed_t chkbaud(int fd);
extern long cvtBaud(speed_t baud);
extern speed_t cvtSpeed(long speed);

#endif
