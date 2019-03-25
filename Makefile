SHELL:=/bin/bash

XML_PATH = grc
LIB_PATH = lib
PY_PATH = python
H_PATH = include/blocksat

MOD_XML = $(shell find $(XML_PATH) -type f -name '*.xml')
MOD_I_H = $(shell find $(LIB_PATH) -type f -name '*.h')
MOD_CC = $(shell find $(LIB_PATH) -type f -name '*.cc')
MOD_PY = $(shell find $(PY_PATH) -type f -name '*.py')
MOD_H = $(shell find $(H_PATH) -type f -name '*.h')
AFF3CT = $(shell find $(LIB_PATH) -type f -name '*.cpp')

HIER_FILES = $(shell find apps/hier/ -type f -name '*.grc')
HIER_PY_FILES = $(patsubst apps/hier/%.grc, python/%.py, $(HIER_FILES))
HIER_RC = $(patsubst apps/hier/%.grc, apps/hier/%.build_record, $(HIER_FILES))

BUILD_DIR = build
BUILD_RC = build_record

.PHONY: build install clean uninstall hier build-hier clean-hier

build: $(BUILD_RC)

$(BUILD_RC): $(MOD_CC) $(MOD_I_H) $(AFF3CT) $(MOD_H) $(MOD_XML) $(MOD_PY)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make
	touch $(BUILD_RC)

install: $(BUILD_RC)
	cd $(BUILD_DIR) && make DESTDIR=$(DESTDIR) install
	-ldconfig

clean:
	rm -f $(BUILD_RC)
	if [ -d $(BUILD_DIR) ]; then $(MAKE) -C $(BUILD_DIR) clean; fi

uninstall:
	rm -f $(BUILD_RC)
	$(MAKE) -C $(BUILD_DIR) uninstall

test:
	$(MAKE) -C $(BUILD_DIR) test

