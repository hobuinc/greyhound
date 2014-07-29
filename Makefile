all:
	$(MAKE) -C request-handler all
	$(MAKE) -C examples/cpp all

clean:
	$(MAKE) -C request-handler clean
	$(MAKE) -C examples/cpp clean
