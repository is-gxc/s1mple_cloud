.PHONY:main
main:main.cpp
	g++ -g -std=c++0x $^ -o $@ -lpthread -lboost_system -lboost_filesystem
