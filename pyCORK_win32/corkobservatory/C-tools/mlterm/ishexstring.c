#include <ctype.h>
int ishexstring(const char *s){
	while (*s){
		if (!isxdigit(*s++))
			return 0;
		//*s = toupper(*s);moved to checkROM	
	}
	return 1;
}

int isnhex(const char *s, int n){
	int i;
	if(n == 0) return 0;
	for (i = 0; i < n; i++){
		if (!isxdigit(*s++))
			return 0;
	}
	return 1;
}
