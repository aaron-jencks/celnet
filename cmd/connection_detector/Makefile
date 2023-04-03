all:
	gcc -g -c -o celnet.o ../../celnet.c
	gcc -g -o ./server ./echo_server.c ./celnet.o -pthread

.PHONY:clean
clean:
	rm ./server ./celnet.o