#include <ctype.h>

#define ConvertHexChar(X)	(isdigit(X) ? (X) - '0' : toupper(X) - 'A' + 0x0A)

// Assumes that the string passed in is exactly two hex digits
unsigned char
hex2int (char *str) {
	//if (!isxdigit(str[0]) || !isxdigit(str[1])) return 0x00;
	return 0x10 * ConvertHexChar (str[0]) + ConvertHexChar (str[1]);
}


#ifdef TEST
main (int c, char **v) {
	for (v++; *v; v++) {
		printf ("0x%02X\n", hex2int (*v));
	}
}
#endif
