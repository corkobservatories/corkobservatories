/* OPNFILNA.C

   This function opens files according to names input by keyboard but
   will not open an existing file to write without confirmation.

*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

int
opnfilna (FILE **fP, char **nP, char *mode, char *Working, char *ext) {
	static char pName[MAXPATHLEN];
	char c[256];
	char *cc;
	
	*nP = pName;
	do{
		pName[0] = '\0';

		if (Working)				// no suggested name
			printf ("Filename [%s.%s]: ", Working, ext);
		else
			printf("Filename: ");
		fflush(stdout);

		fgets(pName, sizeof(pName), stdin);					// read keyboard entry
		if ((cc = strrchr (pName, '\n')) != NULL)
			*cc = '\0';
		if (pName[0] == '\0') {		// nothing entered
			if (Working)
				strcpy (pName, Working);
			else
				return 1;				// quit
		}

		if ((cc = strrchr (pName, '/')) == NULL)
			cc = pName;
		if (strchr (cc, '.') == NULL) {	// if no extension entered...
			strcat (pName, ".");		// add the expected one
			strcat (pName, ext);
		}

		if (mode[0] == 'w') {
        	if (access (pName, F_OK) == 0) {
        		printf(
            	"-------------> %s already exists...continue? (y/n): ",pName);
				fflush(stdout);
				fgets(c, sizeof(c), stdin);
				c[0] = tolower(c[0]);
				while(!(c[0] == 'y' || c[0] == 'n')){
					printf("\ny/n?");
					fflush(stdout);
					fgets(c, sizeof(c), stdin);
					c[0] = tolower(c[0]);
				}
				if(c[0] == 'n'){
					*pName = '\0';
				}
			}
			else if (errno != ENOENT) { 
				printf ("\n\r%s: %s\n\r", pName, strerror(errno));
				fflush (stdout);
				*pName = '\0';
			}
		} //end if mode is write
		*fP=fopen(pName,mode);
		if(!*fP) {
			printf("%s: %s\n", pName, strerror(errno));
			printf("Error in opening file ... try again\n");
		}
	}while (!*fP);
	printf("Opened %s\n", pName);
	return 0;
}
