all: master slave

master:
	g++ -std=c++0x -o master master.cpp
slave:
	g++ -std=c++0x -o slave slave.cpp

clean:
	find . -type f ! -iname "*.cpp" ! -iname "Makefile" ! -iname "README" -delete