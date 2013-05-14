CFLAGS += -Wall -Wno-format-y2k
LDFLAGS += -lm

INSTALL_PATH = /usr/bin/mldat9

mldat9: opnfilna.o read32.o mldat9.o wrt2dat.o wrt2err.o read8.o lost.o hamali.o hamali_2.o slog.o slog_2.o closeallf.o
	$(CC) $(LDFLAGS) -o $@ $^
		
install: mldat9
	cp -f $^ $(INSTALL_PATH)

mldat9.o:

read32.o:

read8.o:

wrt2dat.o:

wrt2err.o:

hamali.o:

hamali_2.o:

slog.o:

slog_2.o:

lost.o:

opnfilna.o:

closeallf.o:

clean:
	rm -rf *.o mldat9 $(INSTALL_PATH)
