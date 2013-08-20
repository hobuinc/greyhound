all:
	$(MAKE) -C pdal-session all
	$(MAKE) -C examples/cpp all

clean:
	$(MAKE) -C pdal-session clean
	$(MAKE) -C examples/cpp clean
