#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mltermfuncs.h"
#include "param.h"

int dump_eeroms (FILE *fp);
static int dump_rtc (FILE *fp);
static int dump_ppcs (FILE *fp);
static int dump_eerom (FILE *fp, const unsigned char *eerom);

/* return 0: success
 *       -1: write error
 *       -2: eerom retrieval error
 */
int
dump_eeroms (FILE *fp) {
	int rtn;

	if ((rtn = dump_rtc (fp)) != 0)
		return rtn;

	if ((rtn = dump_ppcs (fp)) != 0)
		return rtn;

	return 0;
}

/* return 0: success
 *       -1: write error
 *       -2: eerom retreival error
 */
static int
dump_rtc (FILE *fp) {
	if (fputs ("[RTC]\n", fp) < 0)
		return -1;

	return dump_eerom (fp, get_rtc_eerom ());	
}

/* return 0: success
 *       -1: write error
 *       -2: eerom retreival error
 */
static int
dump_ppcs (FILE *fp) {
	const unsigned char *p0;
	const unsigned char *p;
	const unsigned char *ppc_list;
	int identical;
	int rtn;
	int i;

	ppc_list = get_ppc_list ();
	if (!ppc_list || !ppc_list[0])
		return -2;

	p0 = get_ppc_eerom (0);
	if (!p0)
		return -2;

	identical = 1;
	for (i = 1; i < 8 && ppc_list[i]; i++) {
		p = get_ppc_eerom (i);
		if (!p)
			return -2;
		if (memcmp (p0, p, EEROM_BUFSIZ)) {
			identical = 0;
			break;
		}
	}

	if (identical) {
		if (fputs ("[PPC*]\n", fp) < 0)
			return -1;

		return dump_eerom (fp, p0);
	} else {
		for (i = 0; i < 8 && ppc_list[i]; i++) {
			if (fprintf (fp, "[PPC%d]\n", ppc_list[i]) < 0)
				return -1;
			if ((rtn = dump_eerom (fp, get_ppc_eerom (i))) != 0)
				return rtn;
		}
	}

	return 0;
}

/* return 0: success
 *       -1: write error
 *       -2: eerom retreival error
 */
static int
dump_eerom (FILE *fp, const unsigned char *eerom) {
	int i;
	int j;

	if (!eerom)
		return -2;

	for (i = 0; i < EEROM_LINES; i++) {
		if (fprintf (fp, "E%X ", i) < 0)
			return -1;
		for (j = 0; j < EEROM_LINE_LENGTH; j++, eerom++) {
			if (fprintf (fp, "%02X", *eerom) < 0)
				return -1;
		}
		if (i < EEROM_LINES / 2) {
			fputs ("  ", fp);
			for (j = 0, eerom -= EEROM_LINE_LENGTH; j < EEROM_LINE_LENGTH; j++, eerom++)
				fputc (isprint (*eerom) ? *eerom : ' ', fp);
		}
		if (fputc ('\n', fp) < 0)
			return -1;
	}

	return 0;
}
