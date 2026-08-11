NAME = fft_scan
OBJ = $(NAME).o
DEPS =
LIBS = -ldaqhats -lm -lkissfft-float
CFLAGS = -Wall -I/usr/local/include -g
CC = gcc
EXTENSION = .c

all: $(NAME)

$(NAME).o: $(NAME).c
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(DEPS) $(CFLAGS) $(LIBS)

.PHONY:

clean:
	rm -f *.o *~ core $(NAME) fft_scan.csv
