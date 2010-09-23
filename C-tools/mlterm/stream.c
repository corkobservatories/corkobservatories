/* 
	Streaming download

	This download protocol streams bytes out of the flash card, one
	byte after another, for as long as there is data to send.  The data
	are not formatted into packets, and there is no error checking.
	However, the flash sectors are formatted as for the sector-based
	download as indicated below.  If there are no transmission errors,
	the receiver will have a copy of the compact flash sectors, just
	as if they were received by one of the other protocols available.


	[soh][LBA][------data------][crc]

	name  bytes     definition
	soh     1       0x01 (start of header)
	LBA     3       Low 3 bytes of compact flash logical block address
	data  506       ML-CORK data
	crc     2       CRC-16 of data section


	Rules: Receiver starts by sending 'D'.
*/

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>

#include "crc.h"
#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "specialkeys.h"





extern int kfd;
extern int ifd;
extern int nfds;
int stream(int ofd){
	unsigned char Buff[1024];
	unsigned char *pBuff,*pBuffz;
	char ch;
	unsigned char c[5];
	unsigned char cc;
	long int npkt=0;				// number of packets received (good and bad)
	int nto=0;				// number of byte timeouts
	int downloadack=0;			// Dave acknowledged download transfer initiation
	int len;
	struct timeval tv;
	fd_set rfds;
	pBuff=Buff;
	pBuffz=pBuff+512;

	tv.tv_sec=10;
	tv.tv_usec=0;
	
	ch = 'D';
	writeDebug(ifd,&ch,1, "inst: start Dave streaming download");
	FD_ZERO(&rfds);
	FD_SET(ifd, &rfds);
	FD_SET(kfd, &rfds);

	while (nto <= 10 && select(nfds,&rfds,NULL,NULL, &tv) >= 0){
		if (FD_ISSET (kfd, &rfds)) {
			if ((len = read(kfd, c, sizeof(c))) < 0) {
				perror("read");
				restore_tty(kfd);
				close(ofd);
				cancel(4);
				return(1);
			}
			if (isspecialkey(c,len) == QUITLOW) {
				close(ofd);
				cancel(4);
				printf("\n\nDownload aborted.");
				fflush(stdout);
				return(27);
			}
		}	
		else if (FD_ISSET(ifd, &rfds)) {
			if (read(ifd, &cc, 1) < 0) {
				perror("read");
				restore_tty(kfd);
				close(ofd);
				cancel(4);
				return(2);
			}
			if(downloadack==0){
				downloadack = 1;
				printf("download character acknowledged\n");
				fflush(stdout);
			}
			pBuff=Buff;
			switch(cc){				// check for specific codes
				case SOH:			// start of header
				nto = 0;
				break;

				case CR:					// end of transmission
				nto = 0;
				ch = ACK;
				writeDebug(ifd, &ch, 1, "intrument");
				close(ofd);
				printf("\rsectors: %6lx",npkt);
				for(cc=1;cc<3;cc++){
					printf("\7");
					nap (0.5);
				}
				close(ofd);
				return(0);
				break;

				case CAN:   				// transmission aborted
				close(ofd);
				return(24);
				break;

				default:						// none of the above
				nto++;
				Debug("first character is %02X\n",cc);
				Debug("ifd is set but first byte unrecognized: nto = %d\n", nto);
				fflush(stdout);
				break;
			}
			*pBuff++=cc;
			while (pBuffz-pBuff > 0  && nto == 0){
				FD_ZERO(&rfds);
				FD_SET(ifd, &rfds);
				FD_SET(kfd, &rfds);
				tv.tv_sec=2;
				tv.tv_usec=0;
				select(nfds,&rfds,NULL,NULL,&tv); 
				if (FD_ISSET(kfd, &rfds)) {
					if ((len = read(kfd, &c, sizeof(c))) < 0) {
						perror("read");
						close(ofd);
						restore_tty(kfd);
						cancel(4);
						return(2);
					}
					if (c[0] == ESC && len == 1){
						close(ofd);
						cancel(4);
						printf("\n\nDownload aborted.");
						return(27);
					}
				}	
				else if (FD_ISSET(ifd, &rfds)) {
					if (read(ifd, &cc, 1) < 0) {
						perror("read");
						close(ofd);
						restore_tty(kfd);
						cancel(4);
						return(2);
					}
					nto = 0; //currently not necessary
					*pBuff++=cc;
				}
				else{ 
					ch = NAK;
					writeDebug(ifd, &ch, 1, "intrument");
					nto++;
					log_printf(LOG_SCREEN_MIN,"\rsectors: %6lx nto = %d",npkt, nto);
					log_printf(LOG_SCREEN_MIN,"CHECK CABLE CONNECTION");			// beep in case cable fell out
					printf("\7");			// beep in case cable fell out
					fflush(stdout);
				}
				if(nto>10){				// this will never be called within while nto == 0
					log_printf(LOG_SCREEN_MIN,"\nToo many byte timeouts, stopping.");
					close(ofd);
					cancel(4);
					return(4);
				}
			}//end while packet not filled
			if (nto == 0) {
				npkt++;
				// save it
				write(ofd,Buff,512);
				log_printf(LOG_SCREEN_MIN,"\r%6lx",npkt);
			}
		}//end if ISSET ifd 	
		else if (downloadack == 0) {
			ch = 'D';
			writeDebug(ifd,&ch,1, "inst: start Dave streaming download");
			nto++;
			log_printf(LOG_SCREEN_MAX,"download char not ACKed yet: nto = %d\n", nto);
		} else {
			Debug("Mlterm Timeout\n");
			ch = NAK;
			writeDebug(ifd, &ch, 1, "intrument");
			nto++;
			log_printf(LOG_SCREEN_MIN,"\rsectors: %6lx nto = %d",npkt, nto);
			if (nto > 1) { 
				log_printf(LOG_SCREEN_MIN,"CHECK CABLE CONNECTION");			// beep in case cable fell out
				printf("\7");			// beep in case cable fell out
				fflush(stdout);
			}
		}	
		//printf("nto running count: nto = %d\n", nto);
		if (nto > 10){				// this is for line break
			log_printf(LOG_SCREEN_MIN,"\nToo many byte timeouts, stopping.");
			close(ofd);
			cancel(4);
			return(4);
		}
		
		if (nto != 0) tv.tv_sec=10;
		else tv.tv_sec=2;
		tv.tv_usec=0;
		FD_ZERO(&rfds);
		FD_SET(ifd, &rfds);
		FD_SET(kfd, &rfds);
	}//end while
	close(ofd);
	cancel(4);
	return(4);
}	
