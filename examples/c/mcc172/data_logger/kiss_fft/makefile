CFLAGS = -Wall -fPIC -O3 -Dkiss_fft_scalar=float
CC = gcc

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: kiss_fft.o kiss_fftr.o

clean:
	rm -f *~ *.o