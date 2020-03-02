NAME = logger
LIBS = -ldaqhats -pthread

OBJ = logger.o log_file.o errors.o globals.o fft.o kiss_fft/kiss_fftr.o kiss_fft/kiss_fft.o

DEPS = -g globals.h log_file.h errors.h fft.h kiss_fft/kiss_fftr.h kiss_fft/kiss_fft.h

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
	(cd kiss_fft; rm -f *.o)
	rm -f *.o *~ core $(NAME)