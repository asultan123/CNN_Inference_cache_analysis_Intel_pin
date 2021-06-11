build:
	make -C ./pintools/source/tools/Memory/

run:
	pin -t ./pintools/source/tools/Memory/obj-intel64/dcache.so -c 8 -b 32 -a 2 -- ./test