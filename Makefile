EXE_DETECT=server

$(EXE_DETECT): server.c
	gcc -Wall -o server server.c -lpthread

clean:
	rm -f *.o $(EXE_DETECT)

