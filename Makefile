.PHONY = all wii wii-clean  wii-run 

all: wii

clean: wii-clean

run: wii-run 

wii:
	$(MAKE) -f Makefile.ps3

wii-clean:
	$(MAKE) -f Makefile.ps3 clean

wii-run:
	$(MAKE) -f Makefile.ps3 run




