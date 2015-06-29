PROXY?=ON
COMPONENTS = gh_cn
CREDENTIALS = credentials.js
SHELL := /bin/bash

# Directories that need to be copied to the installation path.
SRC_DIRS = frontend-proxy \
		   controller

# Directories where we need to run 'npm install'.
# 'npm install' will also be run at the top level.
NPM_DIRS = examples/js \
		   test \
		   controller \
		   controller/interfaces/ws \
		   controller/interfaces/http

.PHONY: cpp npm test clean install uninstall

all:
	$(MAKE) npm

cpp:
	@echo Building C++ addon.
	$(MAKE) -C controller all

npm:
	@echo Getting NPM dependencies.
	npm install
	$(foreach d, $(NPM_DIRS), cd $(d); npm install; cd -;)

test:
	nodeunit test/unit.js

clean:
	$(MAKE) -C controller clean

install:
	@gh_exists=$$(which greyhound); \
		if [ -x "$$gh_exists" ]; then \
		echo "Stopping running Greyhound components..."; \
		greyhound stop; \
		fi
	@echo Installing Greyhound...
#
# Copy module launchers.
	$(foreach comp, $(COMPONENTS), cp scripts/init.d/$(comp) /etc/init.d/;)
ifeq ($(PROXY),ON)
	@echo Using frontend proxy
	cp scripts/init.d/gh_fe /etc/init.d/
endif
#
# Set up Greyhound component source directory.
	mkdir -p /var/greyhound/
#
# Set up Greyhound component logging directory.
	mkdir -p /var/log/greyhound/
#
# Make source directories for each component.
	$(foreach srcDir, $(SRC_DIRS), mkdir -p /var/greyhound/$(srcDir);)
#
# Copy component sources.
	$(foreach srcDir, $(SRC_DIRS), cp -R $(srcDir)/* /var/greyhound/$(srcDir);)
#
# Copy top-level dependencies.
	cp Makefile /var/greyhound/
	cp config.js /var/greyhound/
ifneq ("$(wildcard $(CREDENTIALS))","")
	cp $(CREDENTIALS) /var/greyhound
endif
	cp forever.js /var/greyhound/
	mkdir -p /var/greyhound/node_modules/
	cp -R node_modules/* /var/greyhound/node_modules/
#
# Copy launcher utility.
	cp greyhound /usr/bin/

uninstall:
	@echo Stopping and removing all traces of Greyhound...
	@greyhound stop
#
# Remove module launchers.
	$(foreach comp, $(COMPONENTS), rm -f /etc/init.d/$(comp);)
	rm -f /etc/init.d/gh_fe
#
# Remove launcher utility.
	rm -f /usr/bin/greyhound
#
# Remove sources.
	rm -rf /var/greyhound/
#
# Remove log files.
	rm -rf /var/log/greyhound/

