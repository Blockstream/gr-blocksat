SHELL:=/bin/bash

XML_PATH = grc
CC_PATH = lib
PY_PATH = python
H_PATH = include/blockstream

MOD_XML = $(shell find $(XML_PATH) -type f -name '*.xml')
MOD_I_H = $(shell find $(CC_PATH) -type f -name '*.h')
MOD_CC = $(shell find $(CC_PATH) -type f -name '*.cc')
MOD_PY = $(shell find $(PY_PATH) -type f -name '*.py')
MOD_H = $(shell find $(H_PATH) -type f -name '*.h')

BUILD_DIR = build
BUILD_RC = build_record

.PHONY: build install clean uninstall

build: $(BUILD_RC)

$(BUILD_RC): $(MOD_CC) $(MOD_I_H) $(MOD_H) $(MOD_XML) $(MOD_PY)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make
	touch $(BUILD_RC)

install: $(BUILD_RC)
	cd $(BUILD_DIR) && make DESTDIR=$(DESTDIR) install

clean:
	rm -f $(BUILD_RC)
	$(MAKE) -C $(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)

uninstall:
	rm -f $(BUILD_RC)
	$(MAKE) -C $(BUILD_DIR) uninstall

