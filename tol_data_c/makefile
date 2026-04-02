NAME = sensor_controller
OBJ = $(NAME).o
LIBS = -ldaqhats -lpthread
CFLAGS = -Wall -I/usr/local/include -g -std=c99 -D_POSIX_C_SOURCE=200809L
CC = gcc
EXTENSION = .c

# Also build the old logger
OLD_NAME = channel4_ringbuffer_logger
OLD_OBJ = $(OLD_NAME).o

all: $(NAME) $(OLD_NAME)

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OLD_NAME): $(OLD_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	@rm -f *.o *~ core $(NAME) $(OLD_NAME)
