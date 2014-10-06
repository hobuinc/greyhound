all:
	$(MAKE) -C session-handler all
	$(MAKE) -C examples/cpp all

clean:
	$(MAKE) -C session-handler clean
	$(MAKE) -C examples/cpp clean

install:
	#
	# Copy module launchers.
	cp init.d/gh_db /etc/init.d/
	chmod 755 /etc/init.d/gh_db
	#
	# Set up Greyhound component source directory.
	mkdir -p /var/greyhound/
	#
	# Set up common module.
	mkdir -p /var/greyhound/common/
	cp -R common/* /var/greyhound/common/
	#
	# Set up database handler module.
	mkdir -p /var/greyhound/db-handler/
	cp -R db-handler/* /var/greyhound/db-handler/
	#
	#
	# TODO
	# Link stuff like /etc/init.d/greyhound/gh_db into /bin for easy start/stop?
	# Make init.d's restart on crash:
	# 		Need to update redis in this case.

uninstall:
	rm -rf /etc/init.d/gh_db
	rm -rf /var/greyhound/
	rm -f /var/log/gh_db

