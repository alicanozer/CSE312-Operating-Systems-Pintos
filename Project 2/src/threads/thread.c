#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-point.h"



#ifdef USERPROG
#include "userprog/process.h"
#endif

/*PART3 ADDED*/
#define NICE_DEFAULT 0
#define NICE_MAX 20
#define NICE_MIN 20 
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0
#define LOCK_SIZE 8

/* Random value for struct thread's `magic' member.
Used to detect stack overflow. See the big comment at the top
of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes. Processes are added to this list
when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
	void *eip; /* Return address. */
	thread_func *function; /* Function to call. */
	void *aux; /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks; /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks; /* # of timer ticks in user programs. */


#define TIME_SLICE 4 /*  Her bir thread icin timer tick. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
 * If true, use multi-level feedback queue scheduler.
 * Controlled by kernel command-line option "-o mlfqs" Advanced Scheduler
 * 
 */
bool thread_mlfqs;

/*PART 3 ADDED*/
int load_avg;



#define MIN_FD 2
#define NO_PARENT -1


static void kernel_thread (thread_func *, void *aux);
static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);





/* Initializes the threading system by transforming the code
	that's currently running into a thread. This can't work in
	general and it is possible in this case only because loader.S
	was careful to put the bottom of the stack at a page boundary.

	Also initializes the run queue and the tid lock.

	After calling this function, be sure to initialize the page
	allocator before trying to create any threads with
	thread_create().

	It is not safe to call thread_current() until this function
	finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
	Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);
  
  /*PART3 ADDED*/
  load_avg = LOAD_AVG_DEFAULT;

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}


void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;
	
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
	PRIORITY, which executes FUNCTION passing AUX as the argument,
	and adds it to the ready queue. Returns the thread identifier
	for the new thread, or TID_ERROR if creation fails.

	If thread_start() has been called, then the new thread may be
	scheduled before thread_create() returns. It could even exit
	before thread_create() returns. Contrariwise, the original
	thread may run for any amount of time before the new thread is
	scheduled. Use a semaphore or some other form of
	synchronization if you need to ensure ordering.

	The code provided sets the new thread's `priority' member to
	PRIORITY, but no actual priority scheduling is implemented.
	Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level stored_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
	Do this atomically so intermediate values for the 'stack'
	member cannot be observed. */
  stored_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (stored_level);

  //PROJECT 2
  // Threadin child process 'i threadin listesine eklenir.
  // normalde process_execute fonksiyonunda bu islemleri yaptım panic hataları
  // olusmaya basladı halbuki pointer ile o anki olusturulan threadi pointliyordum
  // sonunda olusturulurken eklemek istedim bu kısımda eklediğimde sonuc dogru olarak islemleri yaptı
  t->parent = thread_tid(); // parent olarak o anki threadin tid 'sini dondurur.
  struct process *cp = add_process_current_thread(t->tid); // verilen tid li threadin listesine eklenir.
  t->cp = cp; // o anki child process indicator olusturulan child processi gosterir.

  /* Add to run queue. */
  thread_unblock (t);

  /*Add this code*/
  stored_level = intr_disable ();
  /*Eger eklenen thread 'in su an calısan threadten daha buyuk priority e sahip olma
   durumunda yield edilmesi durumları soz konusu olabilir*/
  iscurrent_thread_has_max_priority();
  intr_set_level (stored_level);

  return tid;
}

/* Puts the current thread to sleep. It will not be scheduled
	again until awoken by thread_unblock().

	This function must be called with interrupts turned off. It
	is usually a better idea to use one of the synchronization
	primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
	This is an error if T is not blocked. (Use thread_yield() to
	make the running thread ready.)

	This function does not preempt the running thread. This can
	be important: if the caller had disabled interrupts itself,
	it may expect that it can atomically unblock a thread and
	update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);

  // ready_listesi sort edilir bu sayede bazı işlemlerde ready listesinin
  // en basından en buyuk priority elemanı alabilmemizi sağlayacaktır.
  list_insert_ordered(&ready_list, &t->elem,
	(list_less_func *) &compare_priority,NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
	This is running_thread() plus a couple of sanity checks.
	See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
	If either of these assertions fire, then your thread may
	have overflowed its stack. Each thread has less than 4 kB
	of stack, so a few big automatic arrays or moderate
	recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it. Never
	returns to the caller. */
/**
 *  PROJECT 2
 *  exit() // kill exception durumunda thread_exit cagırılıyor
 *  thread exit olması durumunda child processlerinde exit olması gerekmektedir.Bunun icin burda
 * threadler USER_PROG tanımlanmıssa process_exit cagırılır.
 *
 */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
	and schedule another process. That process will destroy us
	when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU. The current thread is not put to sleep and
	may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
	struct thread *cur = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (cur != idle_thread)
	{
		
		list_insert_ordered(&ready_list, &cur->elem,
		(list_less_func *) &compare_priority,NULL);
	}
	cur->status = THREAD_READY;
	schedule ();
	intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
	This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Idle thread. Executes when no other thread is ready to run.
	The idle thread is initially put on the ready list by
	thread_start(). It will be scheduled once initially, at which
	point it initializes idle_thread, "up"s the semaphore passed
	to it to enable thread_start() to continue, and immediately
	blocks. After that, the idle thread never appears in the
	ready list. It is returned by next_thread_to_run() as a
	special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

	The `sti' instruction disables interrupts until the
	completion of the next instruction, so these two
	instructions are executed atomically. This atomicity is
	important; otherwise, an interrupt could be handled
	between re-enabling interrupts and waiting for the next
	one to occur, wasting as much as one clock tick worth of
	time.

	See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
	7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable (); /* The scheduler runs with interrupts off. */
  function (aux); /* Execute the thread function. */
  thread_exit (); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
	down to the start of a page. Because `struct thread' is
	always at the beginning of a page and the stack pointer is
	somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
	NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);

  /*PART2*/
  /*Initilization for priority donation*/
  t->initial_priority = priority;
  t->waitlock = NULL;
  list_init(&t->list_of_donation);

  /*PART3*/
  /*Initilization for advanced  scheduler */
  t->nice = NICE_DEFAULT;
  t->recent_cpu = RECENT_CPU_DEFAULT;


  /**
   * PROJECT 2
   * */
  list_init(&t->file_list);
  t->fd = MIN_FD;

  list_init(&t->child_list);
  t->cp = NULL;
  t->parent = NO_PARENT;



}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
	returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled. Should
	return a thread from the run queue, unless the run queue is
	empty. (If the running thread can continue running, then it
	will be in the run queue.) If the run queue is empty, return
	idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
	tables, and, if the previous thread is dying, destroying it.

	At this function's invocation, we just switched from thread
	PREV, the new thread is already running, and interrupts are
	still disabled. This function is normally invoked by
	thread_schedule() as its final action before returning, but
	the first time a thread is scheduled it is called by
	switch_entry() (see switch.S).

	It's not safe to call printf() until the thread switch is
	complete. In practice that means that printf()s should be
	added at the end of the function.

	After this function and its caller returns, the thread switch
	is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
	thread. This must happen late so that thread_exit() doesn't
	pull out the rug under itself. (We don't free
	initial_thread because its memory was not obtained via
	palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process. At entry, interrupts must be off and
	the running process's state must have been changed from
	running to some other state. This function finds another
	thread to run and switches to it.

	It's not safe to call printf() until thread_schedule_tail()
	has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
	Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/*   PART 1   */
/* İki thread tick (sleep time ) larinin karsılastırılmasını sağlar*/

bool compare_timeto_wakeup_ticks (const struct list_elem *first,const struct list_elem *second,
	void *aux )

{
	struct thread *t_first = list_entry(first, struct thread, elem);
	struct thread *t_second = list_entry(second, struct thread, elem);
	/*if the first thread has time to wake less than second thread,return true*/
	if (t_first->ticks < t_second->ticks)
	{
		return true;
	}
	/*otherwise return false*/
	return false;
}
/*  PART2   */
/* Sets the current thread's priority to new priority. */
void
thread_set_priority (int new_priority)
{
	if (thread_mlfqs)
	{
		return;
	}
	enum intr_level stored_level = intr_disable ();
	int stored_priority = thread_current()->priority;
	thread_current ()->initial_priority = new_priority;
	
	priority_check_from_donation_list(); // current threadin priority ile
                                        // donation listesindeki priority karsılastırıp
                                          // set ediyoruz.
	// If new priority is greater, donate it
	if (stored_priority < thread_current()->priority)
	{
		priority_donation_solve();
	}
	/*If new priority is less, maybe other threads in ready list have 
	high priority more than current_thread*/
	if (stored_priority > thread_current()->priority)
	{
		iscurrent_thread_has_max_priority();
	}
	intr_set_level (stored_level);
}


/*PART2*/
/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  enum intr_level stored_level = intr_disable ();
  int pri = thread_current()->priority;
  intr_set_level (stored_level);
  return pri;
}


/*PART 2*/
/* Compare to two thread according to priority*/
bool compare_priority (const struct list_elem *first,const struct list_elem *second,
	void *aux )
{
	struct thread *t_first = list_entry(first, struct thread, elem);
	struct thread *t_second = list_entry(second, struct thread, elem);
	
	if (t_first->priority > t_second->priority)
	{
		return true;
	}
	
	return false;
}


/*PART2*/
/*
 * Eger current threadin priority'si buyukse tamamdır
 * eger degılse thread_yield() edilmelidir.
 * bu sekılde ready_listesindeki next max priority li threade gecis yapılır.
 */
	
void iscurrent_thread_has_max_priority (void)
{
	if ( list_empty(&ready_list) )
	{
		return;
	}
	struct thread *t = list_entry(list_front(&ready_list),
	struct thread, elem);
	if (intr_context())
	{
		thread_ticks++;
		if ( thread_current()->priority < t->priority ||
			(thread_ticks >= TIME_SLICE &&
			thread_current()->priority == t->priority) )
		{
			intr_yield_on_return();
		}
		return;
	}
	if (thread_current()->priority < t->priority)
	{
		thread_yield();
	}
}


/*PART2*/
/* current thread  priority inversion sebebiyet olursa donate edilmelidir
 *  bu durum ıcın o an ki thread 'in lock durumuda kontrol edilmeli
 * Aksi halde donation tam olarak sağlanamaz
 */
void priority_donation_solve (void)
{
	int counter = 0;
	struct thread *t = thread_current();
	struct lock *l = t->waitlock;
	while (l && counter < LOCK_SIZE)
	{
		counter++;
		/* lock tutulmuyorsa donationa gerek yok return edilir */
		if (!l->holder)
		{
			return;
		}
		if (l->holder->priority >= t->priority)
		{
			return;
		}
		l->holder->priority = t->priority;
		t = l->holder;
		l = t->waitlock;
	}
}

/*
 *  PART 2
 *  
 *  lock işlemi tamamlanınca artık bekleyen donation listesinde bulunanlar
 *  silinmelidir.Bunun ıcın donation listesinde iterasyonla gezilir,Tamamlanan
 *  lock ile waitlock 'ı esit olanlar karsılastırılarak list_of_donation listesinden
 *  silinir.
 */
void remove_donation_list_waiting_lock(struct lock *lock)
{
	struct list_elem *e = list_begin(&thread_current()->list_of_donation);
	struct list_elem *next;
	while (e != list_end(&thread_current()->list_of_donation))
	{
		struct thread *t = list_entry(e, struct thread, elem_of_donation);
		next = list_next(e);
		if (t->waitlock == lock)
		{
			list_remove(e);
		}
		e = next;
	}
}


/* PART 2
 *
 * update current_thread priority acording to donation_list
 * check_priority_from_donation_list_and_update_priority
 */

void priority_check_from_donation_list (void)
{
	struct thread *t = thread_current();
	t->priority = t->initial_priority;
	if (list_empty(&t->list_of_donation))
	{
		return;
	}
	struct thread *s = list_entry(list_front(&t->list_of_donation),
	struct thread, elem_of_donation);
	if (s->priority > t->priority)
	{
		t->priority = s->priority;
	}
}

/*PART3*/
/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED)
{
  enum intr_level stored_level = intr_disable ();
  thread_current()->nice = nice;
  mlfqs_calculate_priority(thread_current()); // nice set edildiğinde advanced scheduler den dolayı priority check edilmelidir
  iscurrent_thread_has_max_priority();
  intr_set_level (stored_level);
}
/*PART3*/
/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  enum intr_level stored_level = intr_disable ();
  int tmp = thread_current()->nice;
  intr_set_level (stored_level);
  return tmp;
}
/*PART3*/
/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  enum intr_level stored_level = intr_disable ();
  int tmp = float_to_int_r( multiply_mixed(load_avg, 100) );
  intr_set_level (stored_level);
  return tmp;
}
/*PART3*/
/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  enum intr_level stored_level = intr_disable ();
  int tmp = float_to_int_r( multiply_mixed(thread_current()->recent_cpu, 100) );
  intr_set_level (stored_level);
  return tmp;
}

/*PART 3 recent_cpu add by 1*/
void mlfqs_increment_recentcpu (void)
{
  if (thread_current() == idle_thread) // idl_thread ise cik
    {
      return;
    }
  // o anki threadin recent_cpu sini bir arttır
  thread_current()->recent_cpu = add_mixed(thread_current()->recent_cpu, 1);
}

/*PART 3*/
void mlfqs_calculate_priority (struct thread *t)
{
	if (t == idle_thread)
	{
		return;
	}
	int term1 = integer_to_float(PRI_MAX);
	int term2 = division_mixed( t->recent_cpu, 4);
	int term3 = 2*t->nice;
	term1 = subi(term1, term2);
	term1 = sub_mixed(term1, term3);
	t->priority = float_to_int(term1);
	if (t->priority < PRI_MIN)
	{
		t->priority = PRI_MIN;
	}
	if (t->priority > PRI_MAX)
	{
		t->priority = PRI_MAX;
	}
}

/*PART 3
 verilmis olan threadin recent_cpu sunun hesaplanması
 */
void mlfqs_calculation_recent_cpu (struct thread *t)
{
	if (t == idle_thread)
	{
		return;
	}
	int term1 = multiply_mixed(load_avg, 2);
	term1 = division_float(term1, add_mixed(term1, 1) );
	term1 = multiply_float(term1, t->recent_cpu);
	t->recent_cpu = add_mixed(term1, t->nice);
}
/*PART 3
 *  load_avg 'nin set edilmesi
 */
void mlfqs_calculate_load_avg (void)
{
	int term2 = list_size(&ready_list);
	if (thread_current() != idle_thread)
	{
		term2++;
	}
	int term1 = division_mixed(integer_to_float(59), 60);
	term1 = multiply_float(term1, load_avg);
	term2 = division_mixed(integer_to_float(term2), 60);
	load_avg = addi(term1, term2);
	ASSERT (load_avg >= 0)
}

/*PART 3
 *  tum olusturulmus threadlerin recent cpu ve prioritylerinin
 *  hesaplanıp set edilmesi
 */
void mlfqs_calculation_recentcpu_priority (void)
{
  struct list_elem *e;
  for (e = list_begin(&all_list); e != list_end(&all_list);
       e = list_next(e))
    {
      struct thread *t = list_entry(e, struct thread, allelem);
      mlfqs_calculation_recent_cpu(t);
      mlfqs_calculate_priority(t);
    }
}

/**
 *  Verilen pid ye baglı olarak sistem uzerinde all list icerisindeki
 * verilen pid ye baglı olarak esit olan thread var ise true yoksa false
 * return eder.Bu sayede verilen pid ye baglı olarak sistemde threadin
 * yasayıp yasamadıgı hakkında bilgi ediniriz.Bu sekılde child process
 * işlem yaparken o an threadin ölme durumlarını kontrol etmemize yardımcı olur
 *
 **/
bool is_alive (int pid)
{
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      if (t->tid == pid)
        {
        return true;
        }
    }
  return false;
}
