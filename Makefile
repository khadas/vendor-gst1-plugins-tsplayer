all:
	-$(MAKE) -C common
	-$(MAKE) -C audio
	-$(MAKE) -C video

clean:
	-$(MAKE) -C common clean
	-$(MAKE) -C audio clean
	-$(MAKE) -C video clean

install:
	-$(MAKE) -C common install
	-$(MAKE) -C audio install
	-$(MAKE) -C video install

uninstall:
	-$(MAKE) -C common uninstall
	-$(MAKE) -C audio uninstall
	-$(MAKE) -C video uninstall