/* xmocrc    
	The 133 byte packets sent by the serial to compact flash data logger
	(MT01) are formatted as


	[soh][LBA][------data------][crc]

	name  bytes     definition
	soh		1		0x01 (start of header)
	seq#    2		sequence number 
	data  128       ML-CORK data
	crc     2       CRC-16 of data section

	Rules:
	Receiver starts transfer by sending 'C'.  
	Packets are ACK'd or NAK'd depending on the crc result.
	One retry is allowed and is automatically ACK'd.
	The receiver keeps all received packets.
	The first byte in packets with bad crcs is replaced with STX.
	Timeouts-   startup: receiver resends 'C' after 10s
				   byte:    receiver sends NAK 1s after last byte received

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
int xmocrc(int ofd){
	unsigned char Buff[512];
	unsigned char *pBuff,*pBuffz;
	int cZero=0;

	unsigned char c[5];
	unsigned char cc;
	int npkt=0;				// number of packets received (good and bad)
	int nbad=0;				// number of bad pkts received
	int nto=0;				// number of byte timeouts
	int downloadack=0;			// Dave acknowledged download transfer initiation
	int len;
	struct timeval tv;
	fd_set rfds;
	pBuff=Buff;
	pBuffz=pBuff+133;

	tv.tv_sec=10;
	tv.tv_usec=0;
	
	WriteChar(ifd,'C');		// send Shift-c to get going
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
			if (isspecialkey(c, len) == QUITLOW) {
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
				printf("download character acknoledged\n");
				fflush(stdout);
			}
			pBuff=Buff;
			//printf("first character is %02X\n",cc);
			switch(cc){				// check for specific codes
				case SOH:			// start of header
				nto = 0;
				break;

				case 0:				// empty sector
				nto = 0;
				cZero=cZero+1;
				break;

				case EOT:					// end of transmission
				nto = 0;
				WriteChar(ifd,ACK);
				close(ofd);
				printf("\rSeqno: %4d   Block: %6d   Errors: %3d   "
					"Timeouts: %2d   Nulls: %6d",Buff[1],npkt,nbad,nto,cZero);
				//waitms=ElapsedTime();
				//for(cc=1;cc<3;cc++){
					printf("\7");
				//	while(ElapsedTime()-waitms<500*cc);
				//}
				fflush(stdout);
				return(4);
				break;

				case CAN:   				// transmission aborted
				close(ofd);
				return(24);
				break;

				default:						// none of the above
				nto++;
				break;
			}
			*pBuff++=cc;
			while( pBuffz-pBuff > 0  && nto == 0){
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
					nto = 0;
					*pBuff++=cc;
				}
				else{ 
					WriteChar(ifd,NAK);     // char timeout...wake up MT01
					nto++;
					printf("\rSeqno: %4d   Block: %6d   Errors: %3d   "
						"Timeouts: %2d   Nulls: %6d",Buff[1],npkt,nbad,nto,cZero);
					printf("\7\nBEEP 2 second timeout\n");			// beep in case cable fell out
					fflush(stdout);
				}
			}//end while packet not filled
			//printf("end while waiting for packet, nto= %2d\n",nto);
			//fflush(stdout);
			if (nto == 0){	
				//printf("got a packet\n");
				//fflush(stdout);
				npkt++;
				// check it
			//	if ((int)(Buff[1] + Buff[2])==255) {
					if(cal_crc(&Buff[3],130)==0){
						write(ofd,&Buff[3],128);
						WriteChar(ifd,ACK);
					}
			//	}
				else{
					nbad++;
					WriteChar(ifd,NAK);
				}
				printf("\rSeqno: %4d   Block: %6d   Errors: %3d   "
					"Timeouts: %2d   Nulls: %6d",Buff[1],npkt,nbad,nto,cZero);
				fflush(stdout);
			}
		}//end if ISSET ifd 	
		else if (downloadack == 0) {
			WriteChar(ifd,'C');		// send Shift-c to get going
			nto++;
		} else {
			WriteChar(ifd,NAK);     // char timeout...wake up MT01
			nto++;
			printf("\rSeqno: %4d   Block: %6d   Errors: %3d   "
				"Timeouts: %2d   Nulls: %6d",Buff[1],npkt,nbad,nto,cZero);
			printf("\7");			// beep in case cable fell out
		}	
		if (nto > 10) {				// this is for line break
			printf("\nToo many byte timeouts, stopping.");
			close(ofd);
			cancel(4);
			return(4);
		}
		tv.tv_sec=10;
		tv.tv_usec=0;
		FD_ZERO(&rfds);
		FD_SET(ifd, &rfds);
		FD_SET(kfd, &rfds);
	}//end while
	close(ofd);
	cancel(4);
	return(4);
}	
