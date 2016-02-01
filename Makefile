COMPONENTS = gh_cn
CREDENTIALS = credentials.js
KEY=key.pem
CERT=cert.pem
SHELL := /bin/bash

# Directories that need to be copied to the installation path.
SRC_DIRS = controller

# Directories where we need to run 'npm install'.
# 'npm install' will also be run at the top level.
NPM_DIRS = controller \
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
	npm install --unsafe-perm
	$(foreach d, $(NPM_DIRS), cd $(d); rm -rf node_modules; npm install --unsafe-perm; cd -;)

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
	cp scripts/init.d/gh_cn /etc/init.d/
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
ifneq ("$(wildcard $(KEY))","")
	cp $(KEY) /var/greyhound
endif
ifneq ("$(wildcard $(CERT))","")
	cp $(CERT) /var/greyhound
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
	rm -f /etc/init.d/gh_cn
#
# Remove launcher utility.
	rm -f /usr/bin/greyhound
#
# Remove sources.
	rm -rf /var/greyhound/
#
# Remove log files.
	rm -rf /var/log/greyhound/

