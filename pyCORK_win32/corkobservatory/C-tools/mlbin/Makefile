CFLAGS += -Wall

INSTALL_PATH = /usr/bin/mlbin

mlbin: mlbin.o crc.o opnfilna.o 
	$(CC) $(LDFLAGS) -o $@ $^

install: mlbin
	cp -f $^ $(INSTALL_PATH) 

mlbin.o: crc.h

opnfilna.o:

crc.o: crc.h

clean:
	rm -rf *.o mlbin $(INSTALL_PATH)
