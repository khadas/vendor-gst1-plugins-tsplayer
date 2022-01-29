all:
	-$(MAKE) -C common
	-$(MAKE) -C video

clean:
	-$(MAKE) -C common clean
	-$(MAKE) -C video clean

install:
	-$(MAKE) -C common install
	-$(MAKE) -C video install

uninstall:
	-$(MAKE) -C common uninstall
	-$(MAKE) -C video uninstall