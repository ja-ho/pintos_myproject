#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

typedef struct _child_process {
	int pid;
	bool wait;
	bool exit;
	int exit_status;
	int load;
	struct list_elem elem;
} child_process;

typedef struct _process_file {
	struct file *file;
	int fd;
	struct list_elem elem;
} process_file;

void syscall_init (void);

int pibonacci(int n);
int sum_of_four_integers(int a, int b, int c, int d);


#endif /* userprog/syscall.h */
