#include <stdlib.h>
#include <string.h>

// Like strdup, but copies at most n characters.
// If s is longer than n, only n characters are
// copied, and a terminating null is added.
char *
strndup (const char *s, size_t n) {
	char *new;
	int i;

	for (i = 0; s[i] && i < n; i++);

	new = malloc (i + 1);
	if (new) {
		strncpy (new, s, i);
		new[i] = '\0';
	}

	return new;
}
