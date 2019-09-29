CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wextra -Werror -D_BSD_SOURCE
LFLAGS=

default: xmodem_server_test

test: xmodem_server_test
	./xmodem_server_test --xml-output=test-results.xml

infinite_test: xmodem_server_test
	while : ; do ./xmodem_server_test || break ; done

xmodem_server_test: xmodem_server_test.o xmodem_server.o
	$(CC) -o xmodem_server_test xmodem_server_test.o xmodem_server.o

%.o: %.c xmodem_server.h
	cppcheck --quiet $<
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean test infinite_test

clean:
	rm -f *.o xmodem_server_test test-results.xml