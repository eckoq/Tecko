all:
	make -C ./libhttp
	make -C ./wtg
	make -C ./watchdog
	make -C ./ccd clean
	make -C ./ccd
	make -C ./mcd clean
	make -C ./mcd
	make -C ./dcc clean
	make -C ./dcc

clean:
	make -C ./libhttp clean
	make -C ./wtg clean
	make -C ./watchdog clean
	make -C ./ccd clean
	make -C ./mcd clean
	make -C ./dcc clean

love: clean all

