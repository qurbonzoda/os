all: main

main.o: main.c
	gcc -c main.c -o main.o

ifeq (0, 0)
clean: 
	rm main main.o hello.o libhello.a

hello.o: hello.c
	gcc -c hello.c -o hello.o

libhello.a: hello.o
	ar rcs libhello.a hello.o

main: main.o libhello.a
	gcc main.o -o main -L. -lhello
endif

ifeq (0, 1)

clean: 
	rm main main.o hello.o libhello.so

hello.o: hello.c
	gcc -fpic -c hello.c -o hello.o

libhello.so: hello.o
	gcc -shared hello.o -o libhello.so

main: main.o libhello.so
	gcc main.o -o main -Wl,-rpath=/home/qurbonzoda/Programming/os/static_and_dynamic_linking -L. -lhello
endif