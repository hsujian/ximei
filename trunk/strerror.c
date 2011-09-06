#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	int i;
	for (i=1; i<argc; i++) {
		printf("%d => %s\n", atoi(argv[i]), strerror(atoi(argv[i])));
	}
	return 0;
}
