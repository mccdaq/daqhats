NAME = data_logger
COMMON_DIR = ../../common/data_logger
OBJ = data_logger.o $(COMMON_DIR)/log_file.o $(COMMON_DIR)/errors.o
LIBS = -ldaqhats -lgtkdatabox -pthread `pkg-config --libs gtk+-3.0` -lm
CFLAGS = `pkg-config --cflags gtk+-3.0` -I$(COMMON_DIR)
VPATH = $(COMMON_DIR)

CC = gcc

EXTENSION = .c

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

#Cleanup
.PHONY: clean

clean:
	rm -f *.o $(COMMON_DIR)/*.o *~ core $(NAME)
