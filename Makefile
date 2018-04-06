all: setuid

setuid: setuid.c
	gcc -D_GNU_SOURCE -s -o setuid -Wall -O3 setuid.c

clean:
	rm -f setuid
