NAME = logger
OBJ = $(NAME).o fft.o log_file.o errors.o
DEPS = kiss_fft/kiss_fftr.o kiss_fft/kiss_fft.o
LIBS = -ldaqhats -lm -lgtkdatabox -pthread `pkg-config --libs gtk+-3.0`
CFLAGS = -Wall -I/usr/local/include -Ikiss_fft `pkg-config --cflags gtk+-3.0` -g -D_FILE_OFFSET_BITS=64
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
	rm -f *.o *~ core $(NAME)
