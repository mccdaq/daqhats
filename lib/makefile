MAJOR = 1
MINOR = 1
SUB   = 0.3
NAME = daqhats
VERSION = $(MAJOR).$(MINOR).$(SUB)
INSTALL_DIR = /usr/local/lib
BUILD_DIR = ./build

CC = gcc 
CFLAGS = -I../include -I/opt/vc/include -fPIC -Wall -Wextra -g -O2
LDFLAGS = -L/opt/vc/lib -shared -pthread -lm -lbcm_host -Wl,-soname,lib$(NAME).so.$(MAJOR)
RM = rm -f  
TARGET_LIB = lib$(NAME).so.$(VERSION)

SRCS = util.c mcc118.c mcc152.c mcc152_dac.c mcc152_dio.c gpio.c cJSON.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:%.o=%.d)

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET_LIB)

$(BUILD_DIR)/$(TARGET_LIB): $(OBJS)
	@mkdir -p $(@D)
	$(CC) ${LDFLAGS} -o $@ $^

-include $(DEPS)

$(BUILD_DIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@


install:
	@cd ../include; make install; cd ../lib
	@install -D $(BUILD_DIR)/$(TARGET_LIB) $(INSTALL_DIR)
	@ldconfig
	@ln -frs $(INSTALL_DIR)/$(TARGET_LIB) $(INSTALL_DIR)/lib$(NAME).so

uninstall:
	@rm -rf $(INSTALL_DIR)/lib$(NAME).so*
	@ldconfig
	@cd ../include; make uninstall; cd ../lib
	@rm -rf build	

.PHONY: clean

clean:
	@${RM} $(BUILD_DIR)/${TARGET_LIB} ${OBJS} ${DEPS}
	@rmdir $(BUILD_DIR)
