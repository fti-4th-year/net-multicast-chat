all: obj bin bin/run

bin:
	mkdir -p bin

obj:
	mkdir -p obj

bin/run: obj/main.o
	gcc -g $^ -o $@ -lpthread

obj/main.o: src/main.c
	gcc -g -c $< -o $@

clean:
	rm -r ./bin/
	rm -r ./obj/
