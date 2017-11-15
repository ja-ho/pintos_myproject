#include <stdio.h>
#include <stdlibh>
#include <syscall.h>

int main(int argc, char **argv) 
{
	int i;
	int number[4];
	
	printf("%d %d\n", pibonacci(atoi(argv[1])), 
										sum_of_four_integers(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4])));
	return EXIT_SUCCESS;
}

