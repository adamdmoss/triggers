# CC = gcc -Os

example: triggers.o example.o
	$(CC) example.o triggers.o -o example

triggers.o: triggers.c triggers.h
	$(CC) $(CFLAGS) -c triggers.c

example.o: example.c triggers.h
	$(CC) $(CFLAGS) -c example.c

clean:
	$(RM) triggers.o example.o example
