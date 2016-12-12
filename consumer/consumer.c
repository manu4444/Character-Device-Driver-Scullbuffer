/* CSci5103 Fall 2016
* Assignment# 1
* name: Ajay Sundar Karuppasamy, Manu Khandelwal
* student id: 5298653, 5272109
* x500 id: karup002, khand055
* CSELABS machine: csel-x13-umh.cselabs.umn.edu
*/

/*
 * consumer.c
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define BLU   "\x1B[34m"
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define RESET "\x1B[0m"

int main(int argc, char * argv[]) {
	
	if( argc != 3 ){
		printf(RED "usage: consumer number-of-items name\n" RESET);
		exit (1);
	}
	
	
	int total_num_item = atoi(argv[1]);
	char *name = argv[2];
	int scull;
	scull = open("/dev/scullbuffer0", O_RDONLY);
	if (scull == -1) {
		perror("Consumer: Open failed: ");
		exit (1);
	}
	sleep(2);
	char buf[32];
	
	int done = 0;
	int consumed;
	int iters = 0;
	for(; iters<total_num_item; iters++) {
		
		consumed = read(scull, &buf, 32);
		switch (consumed) {
			case -1:
				perror("Reading by consumer failed: ");
				break;
			case 0:
				fprintf(stderr, RED "Buffer is empty and no producers available\n" RESET);
				done = 1;
				break;
			default:
				printf(BLU "Item %d read by Consumer %s: %s\n" RESET, iters+1, name, buf);
				;
		}

        if (done)
			break;
		sleep(2);
	}
	printf(GRN "Total number of Items read by Consumer %s: %d\n" RESET, name, iters);

    // close the scullbuffer
    close( scull );

    // exit
    exit( 0 );
}
