CFLAGS=-g -Wall -pipe
LFLAGS=

default: xmodem_server_test

test: xmodem_server_test
	./xmodem_server_test

infinite_test: xmodem_server_test
	while : ; do ./xmodem_server_test || break ; done

xmodem_server_test: xmodem_server_test.o xmodem_server.o
	$(CC) -o xmodem_server_test xmodem_server_test.o xmodem_server.o

%.o: %.c xmodem_server.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o xmodem_server_test