notapp:
	gcc -std=c99 notapp.c -o notapp -lpthread
notapp.time:
	gcc -std=c99 notapp-time.c -o notapp.time -lpthread

.PHONY: clean
clean: notapp notapp.time

