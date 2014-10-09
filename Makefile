COMPONENTS = gh_db gh_dist gh_sh gh_ws gh_web
SRC_DIRS = common db-handler dist-handler session-handler websocket-handler web

all:
	$(MAKE) -C session-handler all
	$(MAKE) -C examples/cpp all

clean:
	$(MAKE) -C session-handler clean
	$(MAKE) -C examples/cpp clean

install:
#
# Copy module launchers.
	$(foreach comp, $(COMPONENTS), cp init.d/$(comp) /etc/init.d/;)
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
	cp forever.js /var/greyhound/
	mkdir -p /var/greyhound/node_modules/
	cp -R node_modules/* /var/greyhound/node_modules/
#
# TODO
# Link stuff like /etc/init.d/greyhound/gh_db into /bin for easy start/stop?
# Make init.d's restart on crash:
# 		Need to update redis in this case.
#
# TODO TEMPORARY
	cp init.d/gh_pre /etc/init.d/
	mkdir -p /var/greyhound/pre
	mkdir -p /var/greyhound/pre/frontend-proxy
	cp -R frontend-proxy/* /var/greyhound/pre/frontend-proxy

uninstall:
#
# Remove module launchers.
	$(foreach comp, $(COMPONENTS), rm -f /etc/init.d/$(comp);)
#
# Remove sources.
	rm -rf /var/greyhound/
#
# Remove log files.
	rm -rf /var/log/greyhound/
#
# TODO TEMPORARY
	/etc/init.d/gh_pre stop
	rm -f /etc/init.d/gh_pre

