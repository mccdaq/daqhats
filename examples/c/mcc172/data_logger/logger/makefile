NAME = logger
OBJ = $(NAME).o fft.o log_file.o errors.o
DEPS =
LIBS = -ldaqhats -lm -lgtkdatabox -lkissfft-float -pthread `pkg-config --libs gtk+-3.0`
CFLAGS = -Wall -I/usr/local/include `pkg-config --cflags gtk+-3.0` -g -D_FILE_OFFSET_BITS=64
CC = gcc
EXTENSION = .c

all: $(NAME)

$(NAME).o: $(NAME).c
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(DEPS) $(CFLAGS) $(LIBS)

.PHONY:

clean:
	rm -f *.o *~ core $(NAME)
