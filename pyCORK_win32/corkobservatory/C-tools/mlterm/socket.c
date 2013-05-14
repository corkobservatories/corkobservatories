#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

int
netconnect (char *addr, int port) {
	struct sockaddr_in sa;
	struct hostent *he;
	int tos = IPTOS_LOWDELAY;
	int nodelay = 1;
	int errno_sav;
	int fd;

	if ((he = gethostbyname (addr)) == NULL)
		return -1;

	if ((fd = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return -1;

	if ((setsockopt (fd, IPPROTO_IP, IP_TOS, &tos, sizeof (tos)) < 0) ||
	    (setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof (nodelay)) < 0)) {
		errno_sav = errno;
		close (fd);
		errno = errno_sav;
		return -1;
	}

	memset (&sa, 0, sizeof (sa));
	sa.sin_family = AF_INET;
	memcpy (&sa.sin_addr, he->h_addr_list[0], sizeof (sa.sin_addr));
	sa.sin_port = htons (port);

	if (connect (fd, (struct sockaddr *) &sa, sizeof (sa)) < 0) {
		errno_sav = errno;
		close (fd);
		errno = errno_sav;
		return -1;
	}

	return fd;
}
