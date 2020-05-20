CC = gcc
INCLUDE_DIR = ../include
LIB_DIR = ../lib
CFLAGS = -I$(INCLUDE_DIR) -I$(LIB_DIR) -g
OFLAGS = -ldaqhats 
DEPS = $(INCLUDE_DIR)/mcc118.h $(INCLUDE_DIR)/daqhats.h $(LIB_DIR)/mcc118_update.h
INSTALL_DIR = /usr/local/bin
SHORTCUT_DIR = /usr/share/applications
APPS_DIR = /usr/share/mcc/daqhats

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mcc172_firmware_update: mcc172_firmware_update.o
	$(CC) -o $@ $^ $(OFLAGS)

mcc118_firmware_update: mcc118_firmware_update.o
	$(CC) -o $@ $^ $(OFLAGS)

daqhats_list_boards: daqhats_list_boards.o
	$(CC) -o $@ $^ $(OFLAGS)
    
.PHONY: clean

all: mcc118_firmware_update daqhats_list_boards mcc172_firmware_update

install:
	@install -D mcc172_firmware_update $(INSTALL_DIR)
	@install -D mcc118_firmware_update $(INSTALL_DIR)
	@install -D daqhats_read_eeproms $(INSTALL_DIR)
	@install -D daqhats_list_boards $(INSTALL_DIR)
	@install -D daqhats_version $(INSTALL_DIR)
	@install -d $(APPS_DIR)
	@install -D applications/*.py $(APPS_DIR)
	@install -D -m 0644 applications/*.png $(APPS_DIR)
	@install -D -m 0644 shortcuts/* $(SHORTCUT_DIR)

uninstall:
	@rm -f $(INSTALL_DIR)/mcc172_firmware_update
	@rm -f $(INSTALL_DIR)/mcc118_firmware_update
	@rm -f $(INSTALL_DIR)/daqhats_read_eeproms
	@rm -f $(INSTALL_DIR)/daqhats_list_boards
	@rm -f $(INSTALL_DIR)/daqhats_version
	@rm -rf $(APPS_DIR)
	@rm -f $(SHORTCUT_DIR)/mcc*_control_panel.desktop

.DEFAULT_GOAL := all

clean:
	@rm -f *.o *~ core mcc118_firmware_update mcc172_firmware_update
