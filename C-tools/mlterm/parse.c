#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mltermfuncs.h"
#include "param.h"

#define LOG 1

static int parse_params(FILE *fp, struct param_def *pd, const char *label);
static int write_str(FILE *fp, const char *str, const char *prefix, int log);
static int write_rtc_params(FILE *fp);
static int write_ppc_params(FILE *fp);
static int write_mt01_params(FILE *fp);
static int write_rtc_param_errors(FILE *fp);
static int write_ppc_param_errors(FILE *fp);
static int write_mt01_param_errors(FILE *fp);
static void trim(char *buf);
static int label_match(const char *label, const char *buf);
static char *name_val_split(char *buf);

/* Reads parameter file and parses RTC, PPC, and MT01 parameter values.
 *
 * return 0: success
 *        2: parse error
 *        3: immutable field
 *        4: system error
 */
int
parse_all_params (FILE *fp) {
	const unsigned char *ppc_list;
	struct param_def *pd;
	char label[8];
	int ppcnum;
	int rtn;
	int i;

	Debug ("parse_all_params\n");
	if ((rtn = parse_params (fp, pd = get_rtc_param_def (), "RTC")) > 1)
		return rtn;

	Debug ("RTC values:\n");
	for (i = 0; pd[i].name; i++) {
		if (pd[i].userval)
			Debug ("%s: \"%s\"\n", pd[i].name, pd[i].userval);
		else
			Debug ("%s:\n", pd[i].name);
	}

	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return 4;

	DebugBuf (8, ppc_list, "ppc_list:\n");
	for (ppcnum = 0; (ppcnum < 8) && ppc_list[ppcnum]; ppcnum++) {
		sprintf (label, "PPC%d", (int) ppc_list[ppcnum]);
		if ((rtn = parse_params (fp, pd = get_ppc_param_def (ppcnum), label)) > 1)
			return rtn;
		Debug ("PPC%d values:\n", ppc_list[ppcnum]);
		for (i = 0; pd[i].name; i++) {
			if (pd[i].userval)
				Debug ("%s: \"%s\"\n", pd[i].name, pd[i].userval);
			else
				Debug ("%s:\n", pd[i].name);
		}
	}

	if ((rtn = parse_params (fp, pd = get_mt01_param_def (), "MT01")) > 1)
		return rtn;

	Debug ("MT01 values:\n");
	for (i = 0; pd[i].name; i++) {
		if (pd[i].userval)
			Debug ("%s: \"%s\"\n", pd[i].name, pd[i].userval);
		else
			Debug ("%s:\n", pd[i].name);
	}

	return 0;
}

/* return 0: success
 *        1: label not found
 *        2: parse error
 *        3: non-changeable field
 *        4: system error
 */
static int
parse_params (FILE *fp, struct param_def *pd, const char *label) {
	struct param_def *p;
	char buf[BUFSIZ];
	char *val;
	int in_section = 0;
	int rtn = 1;

	if (!pd)
		return 4;

	rewind (fp);
	while (fgets (buf, sizeof (buf), fp)) {
		trim (buf);
		if (*buf) {
			if (*buf == '[') {
				in_section = label_match (label, buf);
				if (in_section)
					rtn = 0;
			} else if (in_section) {
				val = name_val_split (buf);

				// Unrecognized name
				if (!val)
					return 2;

				// Find the param definition for this name
				for (p = pd; p->name && strcasecmp (p->name, buf); p++); 

				if (p->name) { //produce an error if the new userval on these "non changeable parameters" doesn't
								//equal the single valid value. Can potentially get rid of changeable flag.
								//no, I can't. The single valid value is different for each different system for RTC rate tweak and ID#.
								//OR don't store these values as type param_def. Will only print out the ID# (and maybe rate tweak) 
								//in a header of the output file.
					if (!p->changeable)
						return 3;
					if (p->userval)
						free ((void *) p->userval);
					p->userval = strdup (val);
				} else
					return 2;
			}
		}
	}

	return rtn;
}

/* return 0: success
 *       -1: write error
 */
int
write_all_params (FILE *fp) {
/* If a userval exists it will be written instead of the eerom val. */
	int rtn;

	if ((rtn = write_rtc_params (fp)) != 0)
		return rtn;

	if ((rtn = write_ppc_params (fp)) != 0)
		return rtn;

	if ((rtn = write_mt01_params (fp)) != 0)
		return rtn;

	return 0;
}

static int
write_str (FILE *fp, const char *str, const char *prefix, int log) {
	const char *cp;
	const char *nl;
	const char *eol = "\r\n";

	for (cp = str; *cp; ) {
		nl = strchr (cp, '\n');
		if (!nl)
			nl = cp + strlen (cp);
		if (prefix) {
			if (log) {
				if (log_printf (LOG_SCREEN_MAX, prefix) < 0)
					return -1;
			}
			if (fp && (fputs (prefix, fp) < 0))
				return -1;
		}
		if (log) {
			if (log_printf (LOG_SCREEN_MIN, "%.*s%s", nl - cp, cp, eol) < 0)
				return -1;
		}
		if (fp && (fprintf (fp, "%.*s%s", nl - cp, cp, eol) < 0))
			return -1;
		cp = nl;
		if (*cp)
			cp++;
		else
			break;
	}
	return 0;
}

/* return 0: success
 *       -1: write error
 */
static int
write_rtc_params (FILE *fp) {
	struct param_def *p;
	char buf[BUFSIZ];

	if (write_str (fp, "[RTC]\n", NULL, 0) < 0)
		return -1;

	p = get_rtc_param_def ();
	if (!p)
		return -1;

	for (; p->name; p++) {
		if (write_str (fp, "\n", NULL, 0) < 0)
			return -1;
		if (p->comment) {
			if (write_str (fp, p->comment, "# ", 0) < 0)
				return -1;
		}
		sprintf (buf, "%s  %s\n", p->name, p->userval ? p->userval : p->eeromval);
		if (write_str (fp, buf, p->changeable ? NULL : "# ", 0) < 0)
			return -1;
		if (p->error) {
			if (write_str (fp, p->error, p->userval ? "# ERROR: " : "# WARNING: ", 0) < 0)
				return -1;
		}
	}

	return 0;
}

/* return 0: success
 *       -1: write error
 */
static int
write_ppc_params (FILE *fp) {
	const unsigned char *ppc_list;
	struct param_def *p0;
	struct param_def *p;
	char buf[BUFSIZ];
	int label;
	int ppcndx;
	int i;

	if (write_str (fp, "\n\n[PPC*]\n", NULL, 0) < 0)
		return -1;

	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return -1;

	p0 = get_ppc_param_def (0);   //pointer to struct for first ppc (or ppc9)
	if (!p0)
		return -1;

	for (ppcndx = 0; (ppcndx < 8) && ppc_list[ppcndx]; ppcndx++) {
		label = ppcndx ? 1 : 0;
		p = get_ppc_param_def (ppcndx);
		if (!p)
			return -1;

		for (i = 0; p[i].name; i++) {
			if (!ppcndx || i == PPC_META_DATA || strcmp (p0[i].userval ? p0[i].userval : p0[i].eeromval, p[i].userval ? p[i].userval : p[i].eeromval)) {
				if (label) {
					sprintf (buf, "\n\n[PPC%d]\n", (int) ppc_list[ppcndx]);
					if (write_str (fp, buf, NULL, 0) < 0)
						return -1;
					label = 0;
				}
				if (write_str (fp, "\n", NULL, 0) < 0)
					return -1;
				if (p[i].comment) {
					if (write_str (fp, p[i].comment, "# ", 0) < 0)
						return -1;
				}
				sprintf (buf, "%s  %s\n", p[i].name, p[i].userval ? p[i].userval : p[i].eeromval);
				if (write_str (fp, buf, p[i].changeable ? NULL : "# ", 0) < 0)
				if (p[i].error) {
					if (write_str (fp, p[i].error, p[i].userval ? "# ERROR: " : "# WARNING: ", 0) < 0)
						return -1;
				}
			}
		}
	}

	return 0;
}

/* return 0: success
 *       -1: write error
 */
static int
write_mt01_params (FILE *fp) {
	struct param_def *p;
	char buf[BUFSIZ];

	if (write_str (fp, "\n\n[MT01]\n", NULL, 0) < 0)
		return -1;

	p = get_mt01_param_def ();
	if (!p)
		return -1;

	for (; p->name; p++) {
		// If this incarnation of the instrument doesn't support this
		// menu parameter (e.g. LstnMode on older instruments), and 
		// the user hasn't provided a value for it either, then don't
		// include it.
		if (!p->eeromval && !p->userval)
			continue;

		if (write_str (fp, "\n", NULL, 0) < 0)
			return -1;
		if (p->comment) {
			if (write_str (fp, p->comment, "# ", 0) < 0)
				return -1;
		}
		sprintf (buf, "%s  %s\n", p->name, p->userval ? p->userval : p->eeromval);
		if (write_str (fp, buf, p->changeable ? NULL : "# ", 0) < 0)
			return -1;
		if (p->error) {
			if (write_str (fp, p->error, p->userval ? "# ERROR: " : "# WARNING: ", 0) < 0)
				return -1;
		}
	}

	return 0;
}

/* Write out any parameters that have warnings or errors.
 */
int
write_param_errors (FILE *fp) {
	int rtn;

	if ((rtn = write_rtc_param_errors (fp)) != 0) {
		Debug("Error from write_rtc_param_errors: %d (%s)\n", errno, strerror (errno));
		return rtn;
	}

	if ((rtn = write_ppc_param_errors (fp)) != 0) {
		Debug("Error from write_ppc_param_errors: %d (%s)\n", errno, strerror (errno));
		return rtn;
	}

	if ((rtn = write_mt01_param_errors (fp)) != 0) {
		Debug("Error from write_mt01_param_errors: %d (%s)\n", errno, strerror (errno));
		return rtn;
	}

	return 0;
}

static int
write_rtc_param_errors (FILE *fp) {
	struct param_def *p;
	char buf[BUFSIZ];
	int label = 1;

	p = get_rtc_param_def ();
	if (!p)
		return -1;

	for (; p->name; p++) {
		if (p->error) {
			Debug("write_rtc_param_errors: error in %s!\n", p->name);
			if (label) {
				if (write_str (fp, "[RTC]\n", NULL, LOG) < 0)
					return -1;
				label = 0;
			}
			if (write_str (fp, "\n", NULL, LOG) < 0)
				return -1;
			if (p->comment) {
				if (write_str (fp, p->comment, "# ", LOG) < 0)
					return -1;
			}
			sprintf (buf, "%s  %s\n", p->name, p->userval ? p->userval : p->eeromval);
			if (write_str (fp, buf, p->changeable ? NULL : "# ", LOG) < 0)
				return -1;
			if (write_str (fp, p->error, p->userval ? "# ERROR: " : "# WARNING: ", LOG) < 0)
				return -1;
		}
	}

	return 0;
}

static int
write_ppc_param_errors (FILE *fp) {
	const unsigned char *ppc_list;
	struct param_def *p0;
	struct param_def *p;
	char buf[BUFSIZ];
	int ppcndx;
	int label;
	int i;

	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return -1;

	p0 = get_ppc_param_def (0);
	if (!p0)
		return -1;

	for (ppcndx = 0; (ppcndx < 8) && ppc_list[ppcndx]; ppcndx++) {
		label = 1;
		p = get_ppc_param_def (ppcndx);
		if (!p)
			return -1;

		for (i = 0; p[i].name; i++) {
			if (p[i].error) {
				Debug("write_ppc_param_errors: error in %s!\n", p[i].name);
				if (label) {
					sprintf (buf, "\n\n[PPC%d]\n", (int) ppc_list[ppcndx]);
					if (write_str (fp, buf, NULL, LOG) < 0)
						return -1;
					label = 0;
				}
				if (write_str (fp, "\n", NULL, LOG) < 0)
					return -1;

				if (p[i].comment) {
					if (write_str (fp, p[i].comment, "# ", LOG) < 0)
						return -1;
				}
				sprintf (buf, "%s  %s\n", p[i].name, p[i].userval ? p[i].userval : p[i].eeromval);
				if (write_str (fp, buf, p->changeable ? NULL : "# ", LOG) < 0)
					return -1;
				if (write_str (fp, p[i].error, p[i].userval ? "# ERROR: " : "# WARNING: ", LOG) < 0)
					return -1;
			}
		}
	}

	return 0;
}

static int
write_mt01_param_errors (FILE *fp) {
	struct param_def *p;
	char buf[BUFSIZ];
	int label = 1;

	p = get_mt01_param_def ();
	if (!p)
		return -1;

	for (; p->name; p++) {
		if (p->error && (p->eeromval || p->userval)) {
			Debug("write_mt01_param_errors: error in %s!\n", p->name);

			if (label) {
				if (write_str (fp, "\n\n[MT01]\n", NULL, LOG) < 0)
					return -1;
				label = 0;
			}
			if (write_str (fp, "\n", NULL, LOG) < 0)
				return -1;
			if (p->comment) {
				if (write_str (fp, p->comment, "# ", LOG) < 0)
					return -1;
			}
			sprintf (buf, "%s  %s\n", p->name, p->userval ? p->userval : p->eeromval);
			if (write_str (fp, buf, p->changeable ? NULL : "# ", LOG) < 0)
				return -1;
			if (write_str (fp, p->error, p->userval ? "# ERROR: " : "# WARNING: ", LOG) < 0)
				return -1;
		}
	}

	return 0;
}

// Trim comments and leading and trailing whitespace
static void
trim (char *buf) {
	char *s, *e;

	// Find first non-whitespace character
	for (s = buf; isspace (*s); s++);

	// Find last non-comment character
	for (e = s; *e && *e != '#'; e++);
	e--;

	// Backup over any trailing whitespace
	for (; e >= s && isspace (*e); e--);
	e++;

	if (e > s) {
		*e = '\0';
		if (s != buf)
			strcpy (buf, s);
	} else
		*buf = '\0';
}


// Check for label
static int
label_match (const char *label, const char *buf) {
	int blen = strlen (buf);

	// Are we looking at a label?
	if ((buf[0] != '[') || (buf[blen-1] != ']')) {
		return 0;
	}

	// Is it an exact match?
	if ((strlen (label) == blen-2) && !strncasecmp (label, &buf[1], blen-2)) {
		return 1;
	}

	// Is it a wildcarded match?
	if ((buf[blen-2] == '*') && !strncasecmp (label, &buf[1], blen-3)) {
		return 1;
	}

	return 0;
}

// Split "<name><whitespace><val>": null terminate <name>, return <val>
static char *
name_val_split (char *buf) {
	char *s;

	// Find whitespace separator
	for (s = buf; *s && !isspace (*s); s++);

	// Skip over whitespace
	if (*s) {
		*s = '\0';
		for (s++; *s && isspace (*s); s++);
	}

	return *s ? s : NULL;
}

#ifdef TEST
main () {
	struct param_def pd[3];
	int i;

	memset (pd, 0, sizeof (pd));
	pd[0].name = "foo";
	pd[1].name = "bar";

	printf ("parse_params: rtn = %d\n", parse_params (stdin, pd, "label"));

	for (i = 0; pd[i].name; i++) {
		if (pd[i].userval)
			printf ("%s: \"%s\"\n", pd[i].name, pd[i].userval);
	}
}
#endif
