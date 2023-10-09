all:
	cd libs && $(MAKE)
	cd src && $(MAKE)

clean:
	cd libs && $(MAKE) clean
	cd src && $(MAKE) clean
