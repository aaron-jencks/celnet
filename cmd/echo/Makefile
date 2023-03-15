all:
	gcc -g -c -o celnet.o ../../celnet.c
	gcc -g -o ./server ./echo_server.c ./celnet.o

.PHONY:clean
clean:
	rm ./server ./celnet.o