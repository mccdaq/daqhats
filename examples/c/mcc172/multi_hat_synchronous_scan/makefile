NAME = multi_hat_synchronous_scan
OBJ = $(NAME).o
LIBS = -ldaqhats -lm
CFLAGS = -Wall -I/usr/local/include -I./ -g
CC = gcc
EXTENSION = .c

all: $(NAME)

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	@rm -f *.o *~ core $(NAME)
