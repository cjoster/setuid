all: setuid

setuid: setuid.c
<<<<<<< HEAD
	gcc -s -o setuid -Wall -O3 setuid.c
=======
	gcc -o setuid -Wall -O3 setuid.c
>>>>>>> bc975db... Initial commit

clean:
	rm -f setuid
