#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <stdbool.h>

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY, /* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING /* About to be destroyed. */
};

/* Thread identifier type.
You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0 /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63 /* Highest priority. */

/* A kernel thread or user process.

	Each thread structure is stored in its own 4 kB page. The
	thread structure itself sits at the very bottom of the page
	(at offset 0). The rest of the page is reserved for the
	thread's kernel stack, which grows downward from the top of
	the page (at offset 4 kB). Here's an illustration:

	4 kB +---------------------------------+
	| kernel stack |
	| | |
	| | |
	| V |
	| grows downward |
	| |
	| |
	| |
	| |
	| |
	| |
	| |
	| |
	+---------------------------------+
	| magic |
	| : |
	| : |
	| name |
	| status |
	0 kB +---------------------------------+

	The upshot of this is twofold:

	1. First, `struct thread' must not be allowed to grow too
	big. If it does, then there will not be enough room for
	the kernel stack. Our base `struct thread' is only a
	few bytes in size. It probably should stay well under 1
	kB.

	2. Second, kernel stacks must not be allowed to grow too
	large. If a stack overflows, it will corrupt the thread
	state. Thus, kernel functions should not allocate large
	structures or arrays as non-static local variables. Use
	dynamic allocation with malloc() or palloc_get_page()
	instead.

	The first symptom of either of these problems will probably be
	an assertion failure in thread_current(), which checks that
	the `magic' member of the running thread's `struct thread' is
	set to THREAD_MAGIC. Stack overflow will normally change this
	value, triggering the assertion. */
	/* The `elem' member has a dual purpose. It can be an element in
	the run queue (thread.c), or it can be an element in a
	semaphore wait list (synch.c). It can be used these two ways
	only because they are mutually exclusive: only a thread in the
	ready state is on the run queue, whereas only a thread in the
	blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid; /* Thread identifier. */
    enum thread_status status; /* Thread state. */
    char name[16]; /* Name (for debugging purposes). */
    uint8_t *stack; /* Saved stack pointer. */
    int priority; /* Priority. */
    struct list_elem allelem; /* List element for all threads list. */

    /* Shared between thread.c and synch.c ready_list sleep_list*/
    struct list_elem elem; /* List element. */

    /* PART1*/
    int64_t ticks;

    /* PART2 */
    int initial_priority; /*the first priorty --donation*/
    struct lock * waitlock; /*thread is waiting on this lock*/
    struct list list_of_donation; /*donation list*/
    struct list_elem elem_of_donation; /*donation elem list */

    /* PART3 */
    int nice;
    int recent_cpu;



    struct list file_list; // thread file list
    int fd; // file description
    struct list child_list; // child process list
    tid_t parent; // thread parent
    struct process* cp; // thread child process indicator




#ifdef USERPROG
    /* Owned by userprog/process.c. */

    uint32_t *pagedir; /* Page directory. */

    
#endif

    /* Owned by thread.c. */
    unsigned magic; /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
	If true, use multi-level feedback queue scheduler.
	Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

bool is_alive (int pid); // verilen pid ile all list icerisinde
                        // verilen pid ye baglı olarak var olan thread'i aramaktadır.


void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void); // o anki calısan threadin priority'si return edilir.
void thread_set_priority (int); // o anki calısan threadin pri seti yapılır
                                    // eger ready listesinde kendisinden
                                       // prioritysi yuksek olan varsa yield edilir.

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/*PART 1*/
bool compare_timeto_wakeup_ticks (const struct list_elem *first,const struct list_elem *second,
				void *aux ); // threadlerin ticklerinin karsilastırilmasii
/*PART 2*/
bool compare_priority (const struct list_elem *first,const struct list_elem *second,void *aux );
void iscurrent_thread_has_max_priority (void); // current thread ile ready_listesindeki threadleri karsılastırır
                                                // bu durumda daha buyuk bulunursa thread_yield() edilir.
void priority_donation_solve (void);
void remove_donation_list_waiting_lock (struct lock *lock); // o anki calısan threadin lock listesinde
                                                            // parametre olarak verilen lock ' a esit olanlar silinir.
void priority_check_from_donation_list (void);  // donation  listesindekiler priority check edilir

/*PART 3*/
void mlfqs_calculate_priority (struct thread *t); // priority'nin mlfqs algoritmasına gore hesaplanmasını sağlayan fonksyiondur.
void mlfqs_calculation_recent_cpu (struct thread *t); // calculate recent cpu for mlfqs
void mlfqs_calculate_load_avg (void);  // load avg settings
void mlfqs_calculation_recentcpu_priority(void); //tüm thread lerin  icerisindeki recent cpu ve priority calculationı saglar.
void mlfqs_increment_recentcpu (void); // recent_cpu arttırılmasını sağlarız.





#endif /* threads/thread.h */
