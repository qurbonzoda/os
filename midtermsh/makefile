all: midtermsh
clean: 
	rm -f midtermsh midtermsh.o

midtermsh.o: midtermsh.cpp
	g++ -std=c++11 midtermsh.cpp -o midtermsh.o -c
midtermsh: midtermsh.o
	g++ -std=c++11 -s midtermsh.o -o midtermsh
