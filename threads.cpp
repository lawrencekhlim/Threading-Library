// Lawrence Lim
// Threading Project



#include <stdlib.h>        /* for exit() */
#include <stdio.h>         /* standard buffered input/output */
#include <setjmp.h>     /* for performing non-local gotos with setjmp/longjmp */
#include <sys/time.h>     /* for setitimer */
#include <signal.h>     /* for sigaction */
//#include <unistd.h>     /* for pause */
#include <csignal>
#include <pthread.h>
#include <iostream>
/* number of milliseconds to go off */
#define INTERVAL 50
#define MAX_THREADS 129
#define STACK_SIZE 32767

jmp_buf buffers [MAX_THREADS];
bool is_initialized = false;
int current_thread = 0;
bool active [MAX_THREADS];
int* stack_arr [MAX_THREADS];
int num_threads_created = 0;

static int ptr_mangle(int p){
    unsigned int ret;
    asm(" movl %1, %%eax;\n"
        " xorl %%gs:0x18, %%eax;"
        " roll $0x9, %%eax;"
        " movl %%eax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%eax"
        );
    return ret;
}

/* prototype for signal handler will be called when timer goes off */
void signal_handler(int);


/*
 Requires a boolean for handling initialization.
 On the first time running, it must identify the main thread
 and save that position using setjmp. It must also start a timer
 that goes off every 50 milliseconds (and set the alarm as well)
 
 On any execution of pthread_create, pthread_create has to create a new stack,
 place the arguments in reverse order, and then place the return address (which is
 pthread_exit).
 
 ptr_mangle should be able to handle the registers (stack pointer, for instance),
 so that longjmp should work (may have to setlongjmp or something).

 It also helps to have a boolean array telling which threads have exited, and which have not.
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);
/*
Must handle a case where main thread calls pthread_exit.
In that case, the main thread is not deallocated or returned,
it simply waits for other threads to complete.
 
For newly created threads, it should deallocate the stack space created
 by the threading. It should exit the execution here and move on to the next thread.
 It should not return.
 
 */

void pthread_exit (void *value_ptr);


/*
 It should return the currently running thread id (which is the counter
 that cycles through each of the threads.)
 */
pthread_t pthread_self(void);


void initialize () {
    is_initialized = true;
    for (int i = 0; i < MAX_THREADS; i++) {
        active [i] = false;
    }
    active [0] = true;
    current_thread = 0;
    
    // catch signal & start timer
    
    /* itimerval data structure holds necessary info for timer; see man page(s) */
    struct itimerval it_val;
    /* sigaction data structure holds necessary info for signal handling; see man page(s) */
    struct sigaction act;
    /* on signal, call signal_handler function */
    act.sa_handler = signal_handler;
    /* set necessary signal flags; in our case, we want to make sure that we intercept
     signals even when we're inside the signal_handler function (again, see man page(s)) */
    act.sa_flags = SA_NODEFER;
    
    /* register sigaction when SIGALRM signal comes in; shouldn't fail, but just in case
     we'll catch the error  */
    if(sigaction(SIGALRM, &act, NULL) == -1) {
        perror("Unable to catch SIGALRM");
        exit(1);
    }
    
    /* set timer in seconds */
    it_val.it_value.tv_sec = INTERVAL/1000;
    /* set timer in microseconds */
    it_val.it_value.tv_usec = (INTERVAL*1000) % 1000000;
    /* next timer should use the same time interval */
    it_val.it_interval = it_val.it_value;
    
    /* set timer. From now on, after INTERVAL ms SIGALRM will be sent and
     signal_handler will be invoked */
    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        exit(1);
    }
}



//int pthread_create (pthread_t *restrict_thread, const pthread_attr_t * restrict_attr, void *(*start_routine) (void*), void * restrict_arg) {
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg) {
    if (!is_initialized) {
        initialize();
    }
    //std::cout << "Past initialization\n";
    //int* stack = new int [8192];
    int* stack = (int*) (new char [STACK_SIZE]);
    // TODO: arguments come first
    //std::cout << (int)(STACK_SIZE / 4) << "\n"; 
    stack[(int)(STACK_SIZE / 4)] = (int)arg;
    stack[(int)(STACK_SIZE / 4) - 1] = (int)pthread_exit;
    
    int new_thread_id = (num_threads_created) % (MAX_THREADS - 1) + 1;
    bool first_pass = false;

    //bool test = 
    //	 (!first_pass || new_thread_id != (num_threads_created) % (MAX_THREADS - 1) + 1)  && active[new_thread_id];

    //std:: cout << test << "\n";
    for (new_thread_id = (num_threads_created) % (MAX_THREADS - 1) + 1;
	 (!first_pass || new_thread_id != (num_threads_created) % (MAX_THREADS - 1) + 1)  && active[new_thread_id];
	 new_thread_id = (new_thread_id) % (MAX_THREADS -1) + 1 ) {
        first_pass = true;
    }
    
    if (active [new_thread_id]) {
        return 1;
    }

    if (setjmp(buffers[new_thread_id]) == 0) {
        *thread = (pthread_t)(new_thread_id);
        active[new_thread_id] = true;
	num_threads_created += 1;
        stack_arr [new_thread_id] = stack;
        buffers [new_thread_id] -> __jmpbuf[4] = ptr_mangle ((int)(stack+(int)(STACK_SIZE / 4)-1));
        buffers [new_thread_id] -> __jmpbuf[5] = ptr_mangle ((int)start_routine);
	//std::cout << "Past this point as well\n";
        return 0;
    }
    return 0;
}

void change_threads () {
    //std::cout << "Change threads begin\n";
    int curr = current_thread+1;
    for (curr = (current_thread+1) % MAX_THREADS; !active[curr] && curr != current_thread; curr = (curr+1)%MAX_THREADS) {
//	std::cout << "curr = " << curr << "\n";
        ;
    }


//	std::cout << "curr = " << curr << "\n";
    if (curr == current_thread && !active [current_thread]) {
        exit(0);
    }
    else {
        current_thread = curr;
	//std::cout << "Before longjump ";
	//std::cout << current_thread;
	//std::cout << "\n";
        longjmp (buffers[current_thread], 1);
    }
}


void pthread_exit (void *value_ptr) {
    //std::cout << "pthread_exit\n";
    active [current_thread] = false;
    if (current_thread != 0) {
        delete stack_arr[current_thread];
    }
    change_threads();
}

/*
 When a SIGALRM goes off, then it needs to call
  setjmp to save information about registers and
  everything.
 
 We will need a 128 size array of jmp_buf to store
  information on the registers.
 
 We will cycle through the threads in this array
  with a counter (that pthread_self will return).
 
 and longjmp to the incremented counter.
 
 */

/* called when SIGALRM goes off from timer */
void signal_handler(int signo) {
    //std::cout << "Signal handler\n";
    if (setjmp (buffers[current_thread]) == 0) {
        change_threads();
    }
}


pthread_t pthread_self(void) {
    //std::cout << "self begin\n";
    pthread_t p = (pthread_t)current_thread;
    //std::cout << "self end\n";
    return p;
}
