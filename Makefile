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

# Re-build Hierarchical Blocks
# NOTE: the hierarchical blocks are pre-built in the repository due to the fact
# that the top-level Python modules that they generate have been
# customized. They should only be re-built in case there is some
# incompatibility. In this case, the customizations in the top-level Python need
# to be restored. These are easily tracked by using "git diff".
hier: clean-hier build-hier

build-hier: $(HIER_RC)

apps/hier/%.build_record: apps/hier/%.grc
	grcc $<
	mv $(HOME)/.grc_gnuradio/$(*F).py python/
	mv $(HOME)/.grc_gnuradio/$(*F).py.xml grc/blocksat_$(*F).xml
	$(warning Build of hier blocks discards required python customizations)
	$(info Check the changes using git and restore the customizations)
	touch apps/hier/$(*F).build_record

clean-hier:
	-rm $(HIER_PY_FILES)
	-rm $(HIER_RC)
