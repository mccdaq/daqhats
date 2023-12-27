TARGETS = daqhats_manager mcc118_control_panel mcc128_control_panel mcc134_control_panel mcc152_control_panel mcc172_control_panel
LIBS = -ldaqhats -lm
CFLAGS = -Wall -Wno-deprecated -Wno-deprecated-declarations `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0`
CC = gcc
SHORTCUT_DIR = /usr/share/applications
APPS_DIR = /usr/share/mcc/daqhats
ICON_DIR = /usr/share/pixmaps

all: $(TARGETS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

daqhats_manager: daqhats_manager.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 

mcc118_control_panel: mcc118_control_panel.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 

mcc128_control_panel: mcc128_control_panel.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 

mcc134_control_panel: mcc134_control_panel.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 

mcc152_control_panel: mcc152_control_panel.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 

mcc172_control_panel: mcc172_control_panel.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 

.PHONY: all clean

install:
	@install -d $(APPS_DIR)
	@install $(TARGETS) $(APPS_DIR)
	@install -d $(SHORTCUT_DIR)
	@install -m 0644 shortcuts/mcc_daqhats_manager.desktop $(SHORTCUT_DIR)
	@install -m 0644 shortcuts/mcc*_control_panel.desktop $(SHORTCUT_DIR)
	@install -d $(ICON_DIR)
	@install -m 0644 shortcuts/mcc_daqhats.png $(ICON_DIR)

uninstall:
	@rm -f $(ICON_DIR)/mcc_daqhats.png
	@rm -f $(SHORTCUT_DIR)/mcc*_control_panel.desktop
	@rm -f $(SHORTCUT_DIR)/mcc_daqhats_manager.desktop
	@rm -rf $(APPS_DIR)

clean:
	@rm -f *.o *~ core $(TARGETS)
