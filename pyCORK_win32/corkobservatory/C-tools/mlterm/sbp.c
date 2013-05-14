/* SBP    Sector based download
	The 512 byte packets sent by the serial to compact flash data logger
	(MT01) are formatted as


	[soh][LBA][------data------][crc]

	name  bytes     definition
	soh     1       0x01 (start of header)
	LBA     3       Low 3 bytes of compact flash logical block address
	data  506       ML-CORK data
	crc     2       CRC-16 of data section

	Rules:
	Receiver starts transfer by sending 'E'.  
	Packets are ACK'd or NAK'd depending on the crc result.
	One retry is allowed and is automatically ACK'd.
	The receiver keeps all received packets.
	The first byte in packets with bad crcs is replaced with STX.
	Timeouts-   startup: receiver resends 'E' after 10s
				   byte:    receiver sends NAK 1s after last byte received

*/

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#include "crc.h"
#include "mltermfuncs.h"
#include "ttyfuncdecs.h"
#include "specialkeys.h"

#define TIMEOUT 110

static void progress (long int startlba, long int lba, int npkt, int nbad, time_t timeout, int czero, int tot_blocks, struct timeval *start);

extern int kfd;
extern int ifd;
extern int nfds;

int
sbp(int ofd, int tot_blocks) {
	unsigned char Buff[512];
	unsigned char *pBuff;
	int cZero=0;

	unsigned char c[512];
	unsigned char cc;
	long int LBA;
	long int firstLBA = -1;
	int npkt=0;				// number of packets received (good and bad)
	int nbad=0;				// number of bad pkts received
	time_t timeout = 0;		// start time of timeout timer
	int downloadack=0;			// Dave acknowledged download transfer initiation
	int retry=0;				// flag for retry
	int len;
	struct timeval tv;
	fd_set rfds;
	int abort = 0;
	struct timeval start;
	int rtn;

	tv.tv_sec=2;
	tv.tv_usec=0;
	unsigned char ch = 'E';
	writeDebug(ifd,&ch,1,"inst:start sector based download");		// send Shift-e to get going
	gettimeofday (&start, NULL);
	FD_ZERO(&rfds);
	FD_SET(ifd, &rfds);
	FD_SET(kfd, &rfds);

	while ((!timeout || (time(NULL) - timeout <= TIMEOUT)) && (rtn = select(nfds,&rfds,NULL,NULL, &tv)) >= 0){
		//Debug ("select: rtn=%d  nto=%d   isset(kdf)=%d   isset(ifd)=%d\n", rtn, timeout ? time(NULL) - timeout : 0, FD_ISSET (kfd, &rfds), FD_ISSET(ifd, &rfds));
		if (FD_ISSET (kfd, &rfds)) {
			if ((len = readDebug(kfd, c, sizeof(c),"keyboard")) < 0) {
				perror("read");
				close(ofd);
				cancel(4);
				return(1);
			}
			if (isspecialkey(c,len) == QUITLOW) {
				abort = 27;
				log_printf(LOG_SCREEN_MIN, "\n\nDownload aborted between sectors.\n");
				Debug("Download aborted between sectors.\n");
				// No point waiting around for the rest of the block if we're in a timeout situation
				if (timeout) {
					close(ofd);
					cancel(4);
					return abort;
				}
			}
		}	
		else if (FD_ISSET(ifd, &rfds)) {
			if ((len = read(ifd, Buff, sizeof (Buff))) < 0) {
				perror("read");
				close(ofd);
				cancel(4);
				return(2);
			}
			//Debug ("read %d byte(s): first char=0x%02X\n", len, Buff[0]);
			if(downloadack==0){
				if (Buff[0] != 0x01) {
					Debug("Unexpected response to download initiation character.\n");
					log_printf(LOG_SCREEN_MIN, "\nUnexpected response to download initiation character.\nINSTRUMENT LIKELY TIMED OUT.\nReturn to normal baud and press spacebar until prompted by instrument.\n");
					return(4);
				}
				downloadack = 1;
				Debug("download character acknowledged \n");
				log_printf(LOG_SCREEN_MIN, "download character acknowledged \n");
				fflush(stdout);
			}
			switch(Buff[0]){				// check for specific codes
				case SOH:			// start of header
					timeout = 0;
					break;

				case 0x00:				// empty sector
					timeout = 0;
					cZero++;
					break;

				case EOT:					// end of transmission
					timeout = 0;
					ch = ACK;
					writeDebug(ifd,&ch,1,"instrument");
					close(ofd);

					progress (firstLBA, LBA, npkt, nbad, timeout, cZero, tot_blocks, &start);
					//waitms=ElapsedTime();
					Debug("EOT: Download completed\n");
					for(cc=1;cc<3;cc++){
						printf("\7"); 
						nap (0.5);
					}
					close(ofd);
					return(0);
					break;

				case CAN:   				// transmission aborted
					close(ofd);
					Debug("CAN: received from instrument\n");
					return(24);
					break;

				default:						// none of the above
					if (!timeout) {
						timeout = time (NULL) - tv.tv_sec;
						printf("\7"); //FIX change to a writeDebug statement
						Debug("WARNING: data transmission broken between sectors: CHECK CABLE CONNECTION!\n");			// beep in case cable fell out
						log_printf(LOG_SCREEN_MIN, "\nWARNING: data transmission broken between sectors: CHECK CABLE CONNECTION!\n");			// beep in case cable fell out
					}
					Debug("unrecognized first character, nto = %d\n", timeout ? time (NULL) - timeout : 0);
					break;
			}
			pBuff= Buff;
			pBuff += len;
			while ((pBuff-Buff < sizeof (Buff)) && !timeout) {
				Debug ("reading rest of block: %d bytes to go\n", sizeof (Buff) - (pBuff - Buff));
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
						cancel(4);
						return(2);
					}
					if (c[0] == ESC && len == 1){
						abort = 28;
						log_printf(LOG_SCREEN_MIN, "\n\nDownload aborted mid sector.\n");
						Debug("Download aborted mid sector.\n");
						// No point waiting around for the rest of the block if we're in a timeout situation
						if (timeout) {
							close(ofd);
							cancel(4);
							return abort;
						}
					}
				}	
				else if (FD_ISSET(ifd, &rfds)) {
					if ((len = read(ifd, pBuff, sizeof (Buff) - (pBuff - Buff))) < 0) {
						perror("read");
						close(ofd);
						cancel(4);
						return(2);
					}
					//Debug ("read %d bytes\n", len);
					pBuff += len;
					timeout = 0;  // currently this isn't necessary. If have timeouts for gaps within packet later, it will be.
				}
				else{ 
					if (abort) {
						close(ofd);
						cancel(4);
						return(abort);
					} else {
						ch = NAK;
						writeDebug(ifd,&ch,1,"inst: Char timeout, request resend");     // char timeout...wake up MT01
						if (!timeout)
							timeout = time (NULL) - tv.tv_sec;
						progress (firstLBA,LBA, npkt, nbad, timeout, cZero, tot_blocks, &start);
						printf("\7"); //FIX change to a writeDebug statement
						Debug("WARNING: data transmission broken: CHECK CABLE CONNECTION!\n");			// beep in case cable fell out
						log_printf(LOG_SCREEN_MIN, "\nWARNING: data transmission broken: CHECK CABLE CONNECTION!\n");			// beep in case cable fell out
						fflush(stdout);
					}
				}
				if(timeout && (time (NULL) - timeout >= TIMEOUT)){		// this will never be true inside while nto ==0
								//could consider having a different timeout count
								//it would count timeouts, but not cause NAK sent
					log_printf(LOG_SCREEN_MIN, "\nToo many byte timeouts, stopping.\n");
					Debug("Too many byte timeouts, stopping\n.");
					fflush(stdout);
					close(ofd);
					cancel(4);
					return(4);
				}
			}//end while packet not filled
			if (!timeout){
				//printf("got a packet\n");
				//fflush(stdout);
				npkt++;
				// check it
				if(cal_crc(&Buff[4],508)==0){
					write(ofd,Buff,512);
					if (abort) {
						close(ofd);
						cancel(4);
						return(abort);
					}
					ch = ACK;
					//writeDebug(ifd,&ch,1,"instrument"); //this will really clog .dbg file
					WriteChar(ifd,ACK);  
				}
				else{
					nbad++;
					Buff[0]=STX;			// stx replaces soh on bad packets
					write(ofd,Buff,512);
					if (abort) {
						close(ofd);
						cancel(4);
						return(abort);
					}
					if(retry){
						ch = ACK;
						writeDebug(ifd,&ch,1,"inst: ACK bad retry");	// ACK a bad retry
						retry=0;
					}
					else{
						ch = NAK;
						writeDebug(ifd,&ch,1,"inst: bad CRC request resend");
						retry=1;
					}
				}
				LBA=Buff[1]*65536+Buff[2]*256+Buff[3];
				if (firstLBA < 0) firstLBA = LBA;
				progress (firstLBA,LBA, npkt, nbad, timeout, cZero, tot_blocks, &start);
				fflush(stdout);
			}
		}//end if ISSET ifd 	
		else if (abort) {
			close(ofd);
			cancel(4);
			return(abort);
		} else if (downloadack == 0) {
			ch = 'E';
			writeDebug(ifd,&ch,1,"inst: start Earl download protocol");		// send Shift-e to get going
			if (!timeout)
				timeout = time (NULL) - tv.tv_sec;
			Debug("download character not yet acknowledged, try again, nto = %d\n", timeout ? time (NULL) - timeout : 0);
		} else{ //timeout after 10 seconds
		 	ch = NAK;	
			writeDebug(ifd,&ch,1,"instrument");     // nothing from inst when expecting SOH or EOT ...wake up MT01
			if (!timeout)
				timeout = time (NULL) - tv.tv_sec;
			progress (firstLBA, LBA, npkt, nbad, timeout, cZero, tot_blocks, &start);
			printf("\7");			// beep in case cable fell out
			fflush(stdout);
			Debug("WARNING: data transmission still broken: CHECK CABLE CONNECTION!\n");			// beep in case cable fell out
			log_printf(LOG_SCREEN_MIN, "\nWARNING: data transmission still broken: CHECK CABLE CONNECTION!\n");			// beep in case cable fell out
		}	
		if (timeout && (time (NULL) - timeout >= TIMEOUT)){				// this is for line break or non start-of-packet characters.
			log_printf(LOG_SCREEN_MIN, "\nToo many byte timeouts, stopping.\n");
			Debug("Too many byte timeouts, stopping\n.");
			close(ofd);
			cancel(4);
			return(4);
		}
		tv.tv_sec= timeout ? 10 : 5;   //this makes the first warning to user early on 
									   //instead of 10 seconds after the problem occurs.
		tv.tv_usec=0;
		FD_ZERO(&rfds);
		FD_SET(ifd, &rfds);
		FD_SET(kfd, &rfds);
	}//end while
	close(ofd);
    cancel(4);
	return(10);
}

static void
progress (long int startlba, long int lba, int npkt, int nbad, time_t timeout, int czero, int tot_blocks, struct timeval *start) {
	if (tot_blocks) {
		int pct;
		struct timeval now;
		double elapsed;
		int remaining;
		int hh, mm, ss;

		pct = 100 * (lba-startlba+1) / tot_blocks;
		gettimeofday (&now, NULL);
		elapsed = now.tv_sec - start->tv_sec + (now.tv_usec - start->tv_usec) / 1000000.0;
		remaining = elapsed * (tot_blocks - (lba+1)) / (lba+1) + 0.5;
		hh = remaining / ( 60 * 60);
		mm = (remaining / 60) % 60;
		ss = remaining % 60;
		printf("\rLBA: %6lX   Blk: %6d   Err: %3d   TO: %2ld   Nulls: %6d   %3d%%  %02d:%02d:%02d ",
			lba, npkt, nbad, timeout ? time (NULL) - timeout : 0, czero, pct, hh, mm, ss);

	} else
		printf("\rLBA: %6lx   Block: %6d   Errors: %3d   Timeouts: %2ld   Nulls: %6d",
			lba, npkt, nbad, timeout ? time (NULL) - timeout : 0, czero);
}
