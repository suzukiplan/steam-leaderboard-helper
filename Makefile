all:
	make all-`uname`

all-Darwin:
	cp -p ./sdk/redistributable_bin/osx/libsteam_api.dylib .
	g++ -std=c++17 -I./sdk/public/steam -o test test.cpp -L. -lsteam_api
	g++ -std=c++17 -I./sdk/public/steam -o unit_test unit_test.cpp -L. -lsteam_api
	./unit_test
