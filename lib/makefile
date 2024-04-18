MAJOR = 1
MINOR = 5
SUB   = 0.0
NAME = daqhats
VERSION = $(MAJOR).$(MINOR).$(SUB)
INSTALL_DIR = /usr/local/lib
BUILD_DIR = ./build

CC = gcc
CFLAGS = -I../include -I/opt/vc/include -I/usr/include -fPIC -Wall -Wextra -g -O2 -DENABLE_LOCALES=Off
LDFLAGS = -L/opt/vc/lib -zdefs -shared -pthread -Wl,-soname,lib$(NAME).so.$(MAJOR)
DEPLIBS = -lm -lgpiod
RM = rm -f
TARGET_LIB = lib$(NAME).so.$(VERSION)

SRCS = util.c mcc118.c mcc152.c mcc152_dac.c mcc152_dio.c gpio.c cJSON.c mcc134.c mcc134_adc.c nist.c mcc172.c mcc128.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:%.o=%.d)

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET_LIB)

$(BUILD_DIR)/$(TARGET_LIB): $(OBJS)
	@mkdir -p $(@D)
	$(CC) ${LDFLAGS} -o $@ $^ $(DEPLIBS)

-include $(DEPS)

$(BUILD_DIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@


install:
	@cd ../include; make install; cd ../lib
	@install -d $(INSTALL_DIR)
	@install $(BUILD_DIR)/$(TARGET_LIB) $(INSTALL_DIR)
	@ldconfig
	@ln -frs $(INSTALL_DIR)/$(TARGET_LIB) $(INSTALL_DIR)/lib$(NAME).so

uninstall:
	@rm -rf $(INSTALL_DIR)/lib$(NAME).so*
	@ldconfig
	@cd ../include; make uninstall; cd ../lib
	@rm -rf build

.PHONY: clean

clean:
	@${RM} ${OBJS} ${DEPS}
	@rm -rf $(BUILD_DIR)
