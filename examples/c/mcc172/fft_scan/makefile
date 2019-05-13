NAME = fft_scan
OBJ = $(NAME).o
LIBS = -ldaqhats -lm -lfftw3
CFLAGS = -Wall -I/usr/local/include -g
CC = gcc
EXTENSION = .c

all: $(NAME)

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	@rm -f *.o *~ core $(NAME) fft_test.csv
