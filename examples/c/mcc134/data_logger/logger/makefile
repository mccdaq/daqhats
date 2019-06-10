NAME = logger
OBJ = $(NAME).o
LIBS = -ldaqhats -pthread

OBJ = logger.o log_file.o errors.o globals.o

DEPS = -g globals.h log_file.h errors.h

LIBS = -ldaqhats -lgtkdatabox -pthread `pkg-config --libs gtk+-3.0` -lm

CFLAGS = `pkg-config --cflags gtk+-3.0`

CC = gcc

EXTENSION = .c

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

logger: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

#Cleanup
.PHONY: clean

clean:
	rm -f *.o *~ core $(NAME)
