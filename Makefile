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
# Make source directories for each component.
	$(foreach srcDir, $(SRC_DIRS), mkdir -p /var/greyhound/$(srcDir);)
#
# Copy component sources.
	$(foreach srcDir, $(SRC_DIRS), cp -R $(srcDir)/* /var/greyhound/$(srcDir);)
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
# Remove log files.
	$(foreach comp, $(COMPONENTS), rm -f /var/log/$(comp).txt;)
#
# Remove sources.
	rm -rf /var/greyhound/
#
# TODO TEMPORARY
	rm -f /etc/init.d/gh_pre
	rm -f /var/log/gh_pre-hipache.txt
	rm -f /var/log/gh_pre-mongo.txt
	rm -f /var/log/gh_pre-proxy.txt

