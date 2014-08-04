all:
	$(MAKE) -C session-handler all
	$(MAKE) -C examples/cpp all

clean:
	$(MAKE) -C session-handler clean
	$(MAKE) -C examples/cpp clean
