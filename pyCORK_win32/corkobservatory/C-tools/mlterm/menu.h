/*menu.h*/
#ifndef _MENU_H
#define _MENU_H

#define OPT_BAUD	'2'
#define DNLD_BAUD	'3'
#define OPT_TALK	'9'
#define OPT_LSTN	'8'
#define OPT_MODE	'0'

typedef struct {
	char opt;
	char *label;
	char *curr;
	char *dflt;
} MenuItem;

typedef struct {
	char *version;
	char *status;
	long voltage;
	unsigned long mem_used;
	unsigned long mem_tot;
	MenuItem menuItems[64];
	int lenMenu;
} MenuInfo;

#endif
