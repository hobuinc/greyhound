COMPONENTS = gh_cn
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
# Copy sources.
	mkdir -p /var/greyhound
	cp -R controller/ /var/greyhound
	cp config.js /var/greyhound/
	touch /var/log/supervisor/greyhound.log
#
# Copy launcher.
	cp scripts/supervisord.conf /etc/supervisor/conf.d/greyhound.conf
#
ifneq ("$(wildcard $(KEY))","")
	cp $(KEY) /var/greyhound
endif
ifneq ("$(wildcard $(CERT))","")
	cp $(CERT) /var/greyhound
endif
	mkdir -p /var/greyhound/node_modules/
# 	cp -R node_modules/* /var/greyhound/node_modules/
#
# Copy launcher utility.
	cp greyhound /usr/bin/

uninstall:
	@echo Stopping and removing all traces of Greyhound...
	@greyhound stop
#
# Remove launcher.
	rm -f /etc/supervisor/conf.d/greyhound.conf
#
# Remove bin script.
	rm -f /usr/bin/greyhound
#
# Remove sources.
	rm -rf /var/greyhound/

