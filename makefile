all:
	g++ -c -o threads.o threads.cpp -m32
	g++ -o thread_app2 basic_2.c threads.o -m32
	g++ -o thread_app3 basic_3.c threads.o -m32
	g++ -o thread_app1 basic_1.c threads.o -m32
	g++ -o thread_app4 basic_4.c threads.o -m32
