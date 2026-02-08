all:
	gcc -o ./bin/main ./code/main.c -Icode/stavpn/include -Lcode/stavpn/lib
run:
	./bin/main
clean:
	rm -rf ./bin/*
