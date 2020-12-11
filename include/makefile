INSTALL_DIR = /usr/local/include/daqhats

.PHONY: all clean install

all: install
clean: ;

install:
	@install -d $(INSTALL_DIR)
	@install -m0644 *.h $(INSTALL_DIR)

uninstall:
	@rm -rf $(INSTALL_DIR)