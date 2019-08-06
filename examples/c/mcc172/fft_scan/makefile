NAME = fft_scan
OBJ = $(NAME).o
DEPS = kiss_fft/kiss_fftr.o kiss_fft/kiss_fft.o
LIBS = -ldaqhats -lm
CFLAGS = -Wall -I/usr/local/include -Ikiss_fft -g
CC = gcc
EXTENSION = .c

all: kiss_fft $(NAME)

kiss_fft:
	(cd kiss_fft; make all)
	
$(NAME).o: $(NAME).c
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ) 
	$(CC) -o $@ $^ $(DEPS) $(CFLAGS) $(LIBS)

.PHONY: clean kiss_fft

clean:
	(cd kiss_fft; make clean)
	rm -f *.o *~ core $(NAME) fft_scan.csv
