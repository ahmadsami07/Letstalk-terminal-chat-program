makefile:
	gcc -o lets-talk main.c list.c list.h -pthread
	valgrind --leak-check=full ./lets-talk 3000 localhost 3001
	
clean:
	rm lets-talk
	