CFLAGS += -Wall
LDFLAGS += -lm
OSTYPE = $(shell uname -s)

INSTALL_PATH = /usr/bin/mlterm

mlterm: mlterm.o dos2linux.o download.o sbp.o stream.o xmocrc.o crc.o ttyfuncs.o opnfilna.o opnparamfile.o settime.o help.o debug.o termln.o formattime.o talk.o lstn.o lstndouble.o lstnMT01.o MT01funcs.o hex2int.o ishexstring.o rtcppc.o write_eerom_line.o formatdata.o parseIOfile.o validate.o cvt_eerom.o isspecialkey.o dump.o clearserialbuf.o log_printf.o socket.o wake.o menu.o quit.o restore.o usermenu.o par_dnload.o par_upload.o mltermlin.o get_meta.o status.o autodnload.o signals.o strndup.o
	$(CC) $(LDFLAGS) -o $@ $^

install: mlterm
	cp -f $^ $(INSTALL_PATH) 
ifeq ($(OSTYPE),Linux)
	chown root:root $(INSTALL_PATH)
	chmod 4711 $(INSTALL_PATH)
endif

autodnload.o: menu.h usermenu.h ttyfuncdecs.h mltermfuncs.h

clearserialbuf.o: mltermfuncs.h ttyfuncdecs.h

crc.o: crc.h

cvt_eerom.o: mltermfuncs.h param.h

debug.o: mltermfuncs.h

dos2linux.o: mltermfuncs.h

download.o: mltermfuncs.h ttyfuncdecs.h

dump.o: mltermfuncs.h param.h

formattime.o: mltermfuncs.h specialkeys.h usermenu.h

formatdata.o: formatdata.c mltermfuncs.h specialkeys.h usermenu.h ttyfuncdecs.h
	$(COMPILE.c) -Wno-format-y2k $(OUTPUT_OPTION) $*.c

get_meta.o: mltermfuncs.h ttyfuncdecs.h

help.o: mltermfuncs.h

isspecialkey.o: mltermfuncs.h specialkeys.h 

log_printf.o: mltermfuncs.h ttyfuncdecs.h

lstn.o: mltermfuncs.h ttyfuncdecs.h specialkeys.h

lstndouble.o: mltermfuncs.h ttyfuncdecs.h

lstnMT01.o: mltermfuncs.h ttyfuncdecs.h

menu.o: menu.h mltermfuncs.h param.h

mltermlin.o: mltermfuncs.h param.h specialkeys.h 

mlterm.o: mltermfuncs.h ttyfuncdecs.h param.h usermenu.h

MT01funcs.o: mltermfuncs.h ttyfuncdecs.h usermenu.h

opnfilna.o: mltermfuncs.h specialkeys.h

opnparamfile.o: mltermfuncs.h specialkeys.h ttyfuncdecs.h

par_dnload.o: mltermfuncs.h ttyfuncdecs.h

par_upload.o: mltermfuncs.h ttyfuncdecs.h usermenu.h

parseIOfile.o: mltermfuncs.h param.h

quit.o: mltermfuncs.h

restore.o: ttyfuncdecs.h mltermfuncs.h

rtcppc.o: mltermfuncs.h param.h

sbp.o: mltermfuncs.h ttyfuncdecs.h crc.h specialkeys.h

settime.o: mltermfuncs.h ttyfuncdecs.h usermenu.h

signals.o: mltermfuncs.h usermenu.h 

status.o: mltermfuncs.h ttyfuncdecs.h usermenu.h param.h

stream.o: mltermfuncs.h ttyfuncdecs.h crc.h specialkeys.h

talk.o: mltermfuncs.h

termln.o: ttyfuncdecs.h

usermenu.o: mltermfuncs.h usermenu.h

validate.o: mltermfuncs.h param.h ttyfuncdecs.h

write_eerom_line.o: mltermfuncs.h specialkeys.h

xmocrc.o: mltermfuncs.h ttyfuncdecs.h crc.h specialkeys.h

wake.o: mltermfuncs.h ttyfuncdecs.h specialkeys.h usermenu.h

wake.o: mltermfuncs.h

mltermfuncs.h: menu.h
	touch mltermfuncs.h

clean:
	rm -rf *.o mlterm $(INSTALL_PATH)
