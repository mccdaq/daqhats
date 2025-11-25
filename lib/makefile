MAJOR = 1
MINOR = 5
SUB   = 0.1
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

SRCS = util.c mcc118.c mcc152.c mcc152_dac.c mcc152_dio.c cJSON.c mcc134.c mcc134_adc.c nist.c mcc172.c mcc128.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:%.o=%.d)

# Get the full gpiod version string from pkg-config
# The 'shell' function executes the command and stores the output.
GPIOD_VERSION := $(shell pkg-config --modversion libgpiod 2>/dev/null)

# Extract the major version number using another shell call
VERSION_MAJOR := $(shell echo $(GPIOD_VERSION) | cut -d. -f1)

# Start conditional logic within the Makefile
ifeq ($(VERSION_MAJOR), 2)
	# --- V2 Logic ---
	# Define the macro for C code
	SRCS += gpio_v2.c
	MESSAGE = "Building with libgpiod V2 ($(GPIOD_VERSION))"
else ifeq ($(VERSION_MAJOR), 1)
	# --- V1 Logic ---
	SRCS += gpio.c
	MESSAGE = "Building with libgpiod V1 ($(GPIOD_VERSION))"
else
	# --- Error Handling ---
	$(error libgpiod version could not be determined or is not supported. Version: $(GPIOD_VERSION))
endif

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET_LIB)

$(BUILD_DIR)/$(TARGET_LIB): $(OBJS)
	@mkdir -p $(@D)
	$(CC) ${LDFLAGS} -o $@ $^ $(DEPLIBS)

-include $(DEPS)

$(BUILD_DIR)/%.o : %.c
	@echo $(MESSAGE)
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
