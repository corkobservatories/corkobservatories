//parseIOfile.c contains all functions for parsing input parameter file (uploading parameters to instrument).
//it also contains the functions for writing an output parameter file (downloading parameters from instrument).


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mltermfuncs.h"
#include "param.h"

#define LOG 0
#define ParamVal(P)	((P)->userval ? (P)->userval : (P)->eeromval)

static int parse_params(FILE *fp, struct param_def *pd, const char *label);
static int sanity_check(FILE *fp);
static int write_str(FILE *fp, const char *str, const char *prefix, int log);
static int write_rtc_params(FILE *fp);
static int write_ppc_params(FILE *fp);
static int is_identical (int param_ndx);
static int write_param (FILE *fp, const struct param_def *p, int log);
static int write_mt01_params(FILE *fp);
static int write_rtc_param_errors(FILE *fp);
static int write_ppc_param_errors(FILE *fp);
static int write_mt01_param_errors(FILE *fp);
static void trim(char *buf);

static char last_error[8192];

/* parse_all_in_params reads parameter file and parses RTC, PPC, and MT01 parameter values from parameter file.
 *
 * return 0: success
 *        2: parse error
 *        3: non_changable field
 *        4: system error
 */
int
parse_all_in_params (FILE *fp) {
	const unsigned char *ppc_list;
	struct param_def *pd;
	char label[8];
	int ppcnum;
	int rtn;
	int i;

	Debug ("parse_all_params\n");
	*last_error = '\0';

	if ((rtn = sanity_check (fp)) > 1)
		return rtn;

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
	if (!ppc_list) {
		strcpy (last_error, "Error obtaining list of PPCs\n");
		return 4;
	}

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

/* sanity_check scans the parameter file for any misspelled labels,
 * name/value syntax errors, or parameters outside of labeled sections
 *
 * return 0: success
 *        1: no labels
 *        2: parse error
 *        4: system error
 */
static int
sanity_check (FILE *fp) {
	static const char *labelPat = "^\\[(MT01|RTC|PPC[*1-9])\\]$";
	static const char *paramPat = "^([[:alnum:]]+)[[:space:]]+([^[:space:]].*)$";
	const unsigned char *ppc_list;
	regex_t labelReg;
	regex_t paramReg;
	regmatch_t pmatch[3];
	char buf[BUFSIZ];
	int linenum;
	int in_section = 0;
	unsigned char ppcnum;
	int rtn;
	int i;

	// Compile our regexes
    if ((rtn = regcomp (&labelReg, labelPat, REG_EXTENDED | REG_ICASE)) != 0) {
        regerror (rtn, &labelReg, buf, sizeof (buf));
        sprintf (last_error, "Error compiling regex: \"%s\"\n", labelPat);
        return 4;
    }
    if ((rtn = regcomp (&paramReg, paramPat, REG_EXTENDED | REG_ICASE)) != 0) {
        regerror (rtn, &paramReg, buf, sizeof (buf));
        sprintf (last_error, "Error compiling regex: \"%s\"\n", paramPat);
        return 4;
    }

	rewind (fp);
	linenum = 0;
	while (fgets (buf, sizeof (buf), fp)) {
		linenum++;
		trim (buf);
		if (*buf) {
			if (!in_section) {
				// If we haven't entered a labeled section yet, the only valid thing is a label
				if (regexec (&labelReg, buf, NumElm (pmatch), pmatch, 0) == 0) {
					if (!strncmp (&buf[pmatch[1].rm_so], "PPC", 3) && isdigit (buf[pmatch[1].rm_so+3])) {
						ppcnum = buf[pmatch[1].rm_so+3] - '0';
					Debug ("Checking label PPC%d\n", ppcnum);

						// Make sure that the PPC is actually attached to the instrument
						ppc_list = get_ppc_list ();
						if (ppc_list) {
							DebugBuf (8, ppc_list, "ppc_list:\n");
							for (i = 0; (i < 8) && (ppcnum != ppc_list[i]); i++);
							if (i == 8) {
								log_printf (LOG_SCREEN_MIN, "WARNING: Invalid PPC in parameter file section label at line %d: \"%s\"\n", linenum, buf);
								Debug ("PPC%d isn't attached\n", ppcnum);
							} else
								Debug ("PPC%d is attached (ppc_list index %d)\n", ppcnum, i);
						} else
							Debug ("Uh oh, failed to get ppc_list\n");
					}
					in_section = 1;
				} else {
					sprintf (last_error, "Invalid section label in parameter file at line %d: \"%s\"\n", linenum, buf);
					return 2;
				}
			} else {
				// If we're in a labeled section, then either another label or a parameter is valid
				if ((regexec (&labelReg, buf, NumElm (pmatch), pmatch, 0) != 0) &&
					(regexec (&paramReg, buf, NumElm (pmatch), pmatch, 0) != 0)) {
					sprintf (last_error, "Parse error in parameter file at line %d: \"%s\"\n", linenum, buf);
					return 2;
				}
			}
		}
	}

	return in_section ? 0 : 1;
}

const char *
get_last_parse_error (void) {
	return *last_error ? last_error : NULL;
}

/* return 0: success
 *        1: section label not found
 *        2: parse error
 *        3: non-changeable field
 *        4: system error
 */
static int
parse_params (FILE *fp, struct param_def *pd, const char *label) {
	char myLabelPat[32];
	static const char *anyLabelPat = "^\\[(MT01|RTC|PPC[*1-9])\\]$";
	static const char *paramPat = "^([[:alnum:]]+)[[:space:]]+([^[:space:]].*)$";
	regex_t myLabelReg;
	regex_t anyLabelReg;
	regex_t paramReg;
	regmatch_t pmatch[3];
	struct param_def *p;
	char buf[BUFSIZ];
	const char *name;
	int in_section = 0;
	int rtn = 1;
	int linenum;

	if (!pd) {
		sprintf (last_error, "Error obtaining current %s parameter values from instrument\n", label);
		return 4;
	}

	// If we're looking for PPC<n>, then PPC* is also acceptable
	if (!strncmp (label, "PPC", 3))
		sprintf (myLabelPat, "^\\[(PPC\\*|%s)\\]$", label);
	else
		sprintf (myLabelPat, "^\\[(%s)\\]$", label);

	// Compile our regexes
    if ((rtn = regcomp (&myLabelReg, myLabelPat, REG_EXTENDED | REG_ICASE)) != 0) {
        regerror (rtn, &myLabelReg, buf, sizeof (buf));
        sprintf (last_error, "Error compiling regex: \"%s\"\n", myLabelPat);
        return 4;
    }
    if ((rtn = regcomp (&anyLabelReg, anyLabelPat, REG_EXTENDED | REG_ICASE)) != 0) {
        regerror (rtn, &anyLabelReg, buf, sizeof (buf));
        sprintf (last_error, "Error compiling regex: \"%s\"\n", anyLabelPat);
        return 4;
    }
    if ((rtn = regcomp (&paramReg, paramPat, REG_EXTENDED | REG_ICASE)) != 0) {
        regerror (rtn, &paramReg, buf, sizeof (buf));
        sprintf (last_error, "Error compiling regex: \"%s\"\n", paramPat);
        return 4;
    }

	rewind (fp);
	linenum = 0;
	while (fgets (buf, sizeof (buf), fp)) {
		linenum++;
		trim (buf);
		if (*buf) {
			if (!in_section) {
				if (regexec (&myLabelReg, buf, NumElm (pmatch), pmatch, 0) == 0) {
					in_section = 1;
				}
			} else {
				if (regexec (&myLabelReg, buf, NumElm (pmatch), pmatch, 0) == 0) {
					// We found another valid label, e.g. [PPC1] after [PPC*]
				} else if (regexec (&anyLabelReg, buf, NumElm (pmatch), pmatch, 0) == 0) {
					// A label for a different section
					in_section = 0;
				} else if (regexec (&paramReg, buf, NumElm (pmatch), pmatch, 0) == 0) {
					// Find the param definition for this name. for loop stops before p->name reaches null terminus if name is found.
					name = strndup (&buf[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
					for (p = pd; p->name && strcasecmp (p->name, name); p++); 
					free ((void *) name);

					if (p->name) { 
						if (!p->changeable) {
							sprintf (last_error, "Error: attempt to modify non-changeable value for %s:%s at line %d of parameter file\n", label, p->name, linenum);
							return 3;
						}
						if (p->userval)
							free ((void *) p->userval);
						p->userval = strndup (&buf[pmatch[2].rm_so], pmatch[2].rm_eo - pmatch[2].rm_so);
					} else {
						sprintf (last_error, "Error: invalid name at line %d of parameter file: \"%s\"\n", linenum, buf);
						return 2;
					}
				} else {
					sprintf (last_error, "Parse error in parameter file at line %d: \"%s\"\n", linenum, buf);
					return 2;
				}
			}
		}
	}

	if (rtn == 1)
		sprintf (last_error, "Warning: section %s not found in parameter file\n", label);

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
				if (log_printf (LOG_SCREEN_MIN, prefix) < 0)
					return -1;
			}
			if (fp && (fputs (prefix, fp) < 0))
				return -1;
		}
		if (log) {
			if (log_printf (LOG_SCREEN_MIN, "%.*s%s", nl - cp, cp, eol) < 0)
				return -1;
		}
		if (fp) {
			if (fprintf (fp, "%.*s%s", nl - cp, cp, eol) < 0)
				return -1;
			fflush (fp);
		}
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

	if (write_str (fp, "[RTC]\n", NULL, 0) < 0)
		return -1;

	p = get_rtc_param_def ();
	if (!p)
		return -1;

	for (; p->name; p++) {
		if (write_param (fp, p, 0) < 0)
			return -1;
	}

	return 0;
}

/* return 0: success
 *       -1: write error
 */
static int
write_ppc_params (FILE *fp) {
	const unsigned char *ppc_list;
	struct param_def *p;
	char buf[BUFSIZ];
	int label;
	int ppcndx;
	int i;

	ppc_list = get_ppc_list ();
	if (!ppc_list)
		return -1;

	p = get_ppc_param_def (0);   //pointer to struct for first ppc (or ppc9)
	if (!p)
		return -1;

	label = 1;
	for (i = 0; p[i].name; i++) {
		if (is_identical (i)) {
			if (label) {
				if (write_str (fp, "\n\n[PPC*]\n", NULL, 0) < 0)
					return -1;
				label = 0;
			}
			if (write_param (fp, &p[i], 0) < 0)
				return -1;
		}
	}

	for (ppcndx = 0; (ppcndx < 8) && ppc_list[ppcndx]; ppcndx++) {
		p = get_ppc_param_def (ppcndx);
		if (!p)
			return -1;

		label = 1;
		for (i = 0; p[i].name; i++) {
			if (!is_identical (i)) {
				if (label) {
					sprintf (buf, "\n\n[PPC%d]\n", (int) ppc_list[ppcndx]);
					if (write_str (fp, buf, NULL, 0) < 0)
						return -1;
					label = 0;
				}
				if (write_param (fp, &p[i], 0) < 0)
					return -1;
			}
		}
	}

	return 0;
}

// Is the value of this parameter identical for all PPCs?
static int
is_identical (int param_ndx) {
	const unsigned char *ppc_list;
	const struct param_def *p0;
	const struct param_def *p;
	int ppcndx;

	// meta data and calibration coefficients should never be included under [PPC*]
	if (param_ndx == PPC_META_DATA || param_ndx == CALIBRATION_COEFFICIENTS)
		return 0;

	ppc_list = get_ppc_list ();

	// if there's only one PPC, don't use [PPC*] label
	if (!ppc_list || !ppc_list[1])
	 	return 0;

	p0 = get_ppc_param_def (0);
	if (!p0)
		return 0;

	for (ppcndx = 1; (ppcndx < 8) && ppc_list[ppcndx]; ppcndx++) {
		p = get_ppc_param_def (ppcndx);
		if (!p || strcmp (ParamVal (&p0[param_ndx]), ParamVal (&p[param_ndx])))
			return 0;
	}
	return 1;
}

// Write out parameter value in this format:
//
//	# comment
//	name value
//	# warning/error
//
static int
write_param (FILE *fp, const struct param_def *p, int log) {
	char buf[BUFSIZ];

	if (write_str (fp, "\n", NULL, log) < 0)
		return -1;
	if (p->comment) {
		if (write_str (fp, p->comment, "# ", log) < 0)
			return -1;
	}
	sprintf (buf, "%s  %s\n", p->name, ParamVal (p));
	if (write_str (fp, buf, p->changeable ? NULL : "# ", log) < 0)
		return -1;
	if (p->error) {
		if (write_str (fp, p->error, p->userval ? "# ERROR: " : "# WARNING: ", log) < 0)
			return -1;
	}

	return 0;
}

/* return 0: success
 *       -1: write error
 */
static int
write_mt01_params (FILE *fp) {
	struct param_def *p;

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

		if (write_param (fp, p, 0) < 0)
			return -1;
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
			if (write_param (fp, p, LOG) < 0)
				return -1;
		}
	}

	return 0;
}

static int
write_ppc_param_errors (FILE *fp) {
//this function doesn't really need to get all the new eeroms since 
//it is just checking the parameters in the param_def struct, however, 
//to scroll through all the necessary PPC param_defs, it needs to know 
//how many PPCs there are, which requires it to do the probe_ppc 
//business from the get_ppc_list call
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
				if (write_param (fp, &p[i], LOG) < 0)
					return -1;
			}
		}
	}

	return 0;
}

static int
write_mt01_param_errors (FILE *fp) {
	struct param_def *p;
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
			if (write_param (fp, p, LOG) < 0)
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

	// Is this a comment?
	if (*s == '#') {
		e = s;
	} else {
		// Backup over any trailing whitespace
		for (e = s + strlen (s) - 1; e >= s && isspace (*e); e--);
		e++;
	}

	if (e > s) {
		*e = '\0';
		if (s != buf)
			strcpy (buf, s);
	} else
		*buf = '\0';
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
