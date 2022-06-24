NAME = logger
OBJ = $(NAME).o log_file.o errors.o
LIBS = -ldaqhats -lgtkdatabox -pthread `pkg-config --libs gtk+-3.0` -lm
CFLAGS = `pkg-config --cflags gtk+-3.0` -D_FILE_OFFSET_BITS=64

CC = gcc

EXTENSION = .c

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

#Cleanup
.PHONY: clean

clean:
	rm -f *.o *~ core $(NAME)
