all:clean
	make -C ./base clean
	make -C ./base
	make -C ./libhttp clean
	make -C ./libhttp
	make -C ./watchdog clean
	make -C ./watchdog
	make -C ./ccd clean
	make -C ./ccd
	make -C ./mcd clean
	make -C ./mcd
	make -C ./dcc clean
	make -C ./dcc
	make -C ./tools clean
	make -C ./tools
	make -C ./common clean
	make -C ./common

clean:
	make -C ./base clean
	make -C ./libhttp clean
	make -C ./watchdog clean
	make -C ./ccd clean
	make -C ./mcd clean
	make -C ./dcc clean
	make -C ./tools clean
	make -C ./common clean
	find . -name "*.gcno" | xargs rm -rf 
	find . -name "*.gcda" | xargs rm -rf

love: clean all

#package
pack:
	cd ../package;bash ./package.sh
