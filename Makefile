EXE_DETECT=server

$(EXE_DETECT): server.c
	gcc -Wall -o server server.c
