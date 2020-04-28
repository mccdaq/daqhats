NAME = data_logger
COMMON_DIR = ../../common/data_logger
OBJ = $(NAME).o fft.o $(COMMON_DIR)/log_file.o $(COMMON_DIR)/errors.o
DEPS = kiss_fft/kiss_fftr.o kiss_fft/kiss_fft.o
LIBS = -ldaqhats -lm -lgtkdatabox -pthread `pkg-config --libs gtk+-3.0`
CFLAGS = -Wall -I/usr/local/include -Ikiss_fft `pkg-config --cflags gtk+-3.0` -g -I$(COMMON_DIR)
VPATH = $(COMMON_DIR)
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
	rm -f *.o $(COMMON_DIR)/*.o *~ core $(NAME)
