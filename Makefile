TARGET: testhttp_raw

CC	= cc
CFLAGS	= -Wall -O2
LFLAGS	= -Wall

testhttp_raw.o err.o: err.h

testhttp_raw: testhttp_raw.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f testhttp_raw *.o *~ *.bak
