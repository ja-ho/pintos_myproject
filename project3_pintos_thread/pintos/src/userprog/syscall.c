#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "lib/string.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include <user/syscall.h>
#include "filesys/file.h"
#include "filesys/filesys.h"


static void syscall_handler (struct intr_frame *);


bool check_address(void *esp);
void get_argument(struct intr_frame *f, int *arg, int length);
int user_string_ptr(void *vaddr);
void halt(void );
void exit(int status);
pid_t exec (const char *file);
int wait(tid_t tid);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
struct file* get_file(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);


struct lock lock_filesys;

void
syscall_init (void) 
{
	lock_init(&lock_filesys);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int arg[3];
	if(!check_address(f->esp)){
		exit(-1);
	}
	int handler_number = *(int *)(f->esp);
switch(handler_number)
	{
		case SYS_HALT :
				halt();
				break;
		case SYS_EXIT :

				exit(*(int *)(f->esp+4));
				break;
		case SYS_EXEC :
				//may be, requires more strict synchronization this part
				
				if(!check_address(f->esp+4)) {
					f->eax = -1;
				}
				arg[0] = *((int *)(f->esp+4));
				f->eax = exec((const char *) arg[0]);
				break;
		case SYS_WAIT :
				f->eax = wait(*(pid_t*)(f->esp+4));
				break;
		case SYS_READ :
				if(!check_address(f->esp+8)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				arg[1] = *((int *)(f->esp+8));
				arg[2] = *((int *)(f->esp+12));
				f->eax = read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
				break;
		case SYS_WRITE :
				if(!check_address(f->esp+8)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				arg[1] = *((int *)(f->esp+8));
				arg[2] = *((int *)(f->esp+12));
				f->eax = write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
				break;	
				//hex_dump((unsigned int)(f->esp), f->esp, 0x100, true);
		//case SYS_PIBO:
		//		get_argument(f, &arg[0], 2);	
		//		f->eax = pibonacci(arg[1]);
		//		break;

		//case SYS_SUM:
		//		get_argument(f, &arg[0], 4); 
		//		f->eax = sum_of_four_integers(
					
		//		break;
		case SYS_CREATE :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				arg[1] = *((int *)(f->esp+8));
				lock_acquire(&lock_filesys);
				f->eax = create((const char*)arg[0], (unsigned)arg[1]);
				lock_release(&lock_filesys);
				break;
		case SYS_REMOVE :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				lock_acquire(&lock_filesys);
				f->eax = remove((const char *)arg[0]);
				lock_release(&lock_filesys);
				break;
		case SYS_OPEN :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				lock_acquire(&lock_filesys);
				f->eax = open((const char *)arg[0]);
				break;
		case SYS_FILESIZE :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				lock_acquire(&lock_filesys);
				f->eax = filesize(arg[0]);
				break;
		case SYS_SEEK :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				arg[1] = *((int *)(f->esp+8));
				lock_acquire(&lock_filesys);
				seek(arg[0], (unsigned)arg[1]);
				break;
		case SYS_TELL :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				lock_acquire(&lock_filesys);
				f->eax = tell(arg[0]);
				break;
		case SYS_CLOSE :
				if(!check_address(f->esp+4)) {
					exit(-1);
				}
				arg[0] = *((int *)(f->esp+4));
				lock_acquire(&lock_filesys);
				close(arg[0]);
				break;
	}
}

bool check_address(void *esp)
{
	//need check
	//is exists?
	if(!(is_user_vaddr(esp)) || pagedir_get_page((uint32_t *)(thread_current()->pagedir), (void *)esp) == NULL) {
		return false;
	}
	//is valid?
	else if((unsigned int)esp < 0x08048000 || esp >PHYS_BASE) {
		return false;
	}
	return true;
}

void halt(void)
{
	if(lock_held_by_current_thread(&lock_filesys)) {
		lock_release(&lock_filesys);
	}
	shutdown_power_off();
}

void exit (int status) 
{
	if(lock_held_by_current_thread(&lock_filesys)) {
		lock_release(&lock_filesys);
	}
	struct thread *current = thread_current();
	current->exit_status = status;
	if(status < 0) {
		current->exit_status = -1;
	}
	printf("%s: exit(%d)\n", current->name, current->exit_status);
	thread_exit();
}

pid_t exec (const char *file)
{
	pid_t pid = process_execute(file);
	struct list_elem *e;
	struct thread *parent = thread_current();
	child_process *childProcess = NULL;
	for(e = list_begin(&parent->child_list); e != list_end(&parent->child_list); e=list_next(e)) {
		childProcess = list_entry(e, child_process, elem);
		if(pid == childProcess->pid) {
			break;
		}
	}
	
	if(childProcess->load == 2) { //0 은 NOT LOADED, 2는 load fail
		return -1;
	}
	while(childProcess->load == 0) {
		barrier();
	} 

	return pid;
}

int wait (pid_t pid)
{
	return process_wait(pid);
}

//need to implement
int read(int fd, void *buffer, unsigned size)
{
	off_t bytes;
	int i;
	uint8_t *temp_buffer = (uint8_t *)buffer;
	if(fd == 0) { //STDIN_FILENO == 0  in stdio.h
		for(i=0; i<(signed)size; i++) {
			temp_buffer[i] = input_getc();	
		}
		//input_getc();//	may be need to fix
		return size;
	} else {
		lock_acquire(&lock_filesys);
		struct file *tempFile = get_file(fd);
		if(!tempFile) {
			lock_release(&lock_filesys);
			return -1;
		}
		file_deny_write(tempFile);
		bytes = file_read(tempFile, buffer, size);
		lock_release(&lock_filesys);
		return bytes;
	}
}

int write(int fd, const void *buffer, unsigned size)
{
	off_t bytes;
	if(fd == 1) {	//STDOUT_FILENO == 1  in stdio.h
		putbuf(buffer, size);
		return size;
	} else {
		lock_acquire(&lock_filesys);
		struct file *tempFile = get_file(fd);
		if(!tempFile) {
			lock_release(&lock_filesys);
			return -1;
		}
		bytes = file_write(tempFile, buffer, size);
		file_deny_write(tempFile);
		lock_release(&lock_filesys);
		return bytes;
	}
		
}



int pibonacci(int n)
{
	int a, b, c, i, temp;
	if(n==1) return 1;
	if(n==2) return 1;
	a = b = 1;
	c = a+b;
	for(i=0; i<n-2; i++) {
		temp = b;
		b=a;
		a=temp;
		c=a+b;
	}
	return c;
}

int sum_of_four_integers(int a, int b, int c, int d)
{
	return a+b+c+d;
}

bool create(const char *file, unsigned initial_size)
{
	char *checkTemp;
	bool success;
	checkTemp = (char *)file;
	//if null, bad-ptr
	if(!check_address(checkTemp)) {
		exit(-1);
	}
	//if create-empty
	if(strlen(file) == 0) {
		exit(-1);
	}

	//}
	success = filesys_create(file, initial_size);
	return success;
	//create done!
}

bool remove(const char *file)
{
	bool success = filesys_remove(file);
	return success;
}

int open(const char *file)
{
	process_file *tempFile;	
	struct file *f = filesys_open(file);
	if(!f) {
		lock_release(&lock_filesys);
		return -1;
	}
	tempFile = (process_file *)malloc(sizeof(process_file));
	tempFile->fd = thread_current()->fd;
	tempFile->file = f;
	list_push_back(&thread_current()->file_list, &tempFile->elem);
	thread_current()->fd++;
	lock_release(&lock_filesys);
	return tempFile->fd;
}

int filesize(int fd)
{
	int size;
	struct file *fileTemp = get_file(fd);
	if(!fileTemp) {
		lock_release(&lock_filesys);
		return -1;
	}
	size = file_length(fileTemp);
	lock_release(&lock_filesys);
	return size;
}

struct file* get_file(int fd)
{
	struct thread *temp = thread_current();
	process_file *returnTemp;
	struct list_elem *e;
	bool find=false;
	for(e=list_begin(&temp->file_list); e!=list_end(&temp->file_list); e=list_next(e)) {
		process_file *fileTemp = list_entry(e, process_file, elem);
		if(fd == fileTemp->fd) {
			find=true;
			returnTemp = fileTemp;
			break;
		}
	}

	if(find == true) {
		return returnTemp->file;
	}
	return NULL;
}

void seek(int fd, unsigned position)
{
	struct file *fileTemp = get_file(fd);
	if(!fileTemp) {
		lock_release(&lock_filesys);
		return;
	}
	file_seek(fileTemp, position);
	lock_release(&lock_filesys);
}
unsigned tell(int fd)
{
	int position;
	struct file *fileTemp = get_file(fd);
	if(!fileTemp) {
		lock_release(&lock_filesys);
		return -1;
	}
	position = file_tell(fileTemp);
	lock_release(&lock_filesys);
	return position;
}
void close(int fd)
{
	struct thread *cur = thread_current();
	struct list_elem *e;
	process_file *fileTemp;
	for(e=list_begin(&cur->file_list); e!=list_end(&cur->file_list); e=list_next(e)) {
		fileTemp = list_entry(e, process_file, elem);
		if(fd == fileTemp->fd) {
			list_remove(&fileTemp->elem);
			file_close(fileTemp->file);
			free(fileTemp);
			lock_release(&lock_filesys);
			return;
		}
	}

	lock_release(&lock_filesys);
}

