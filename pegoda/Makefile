
all: pegoda

pegoda: pegoda.o
	$(CC) -lusb -o $@ $^

clean:
	rm -f pegoda *.o
