
/*Thr 4 Apr 2015 2:20:57 PM EET
* GEBZE TECHNICAL UNIVERSITY
* DEPARTMENT OF COMPUTER ENGINEERING
* CSE 312 OPERATING SYSTEM
* 2014-2015 SPRİNG
* PROJECT 1b
* @date 04/04/2015
* @author Alican OZER-Selim AKSOY
*/
/*------------------thread.h-------------------- */
/*thread structure na eklenenler*/

    uint64_t num_of_tickets;//ticket sayısı
    uint64_t initial_tickets;//initial ticket sayısı
    int *threadTickets;//ticketların listesi    
    
int *allTickets;//tüm ticketların listesi
tid_t *allTids;//hangi ticketın hangi threade ait olduğunun listesi
int allused=0;//toplam kullanılan ticket sayısı
    
uint32_t MULTIPLIER_OLD = 4;//eski ticket sayı çatpanı
uint32_t MULTIPLIER_NEW = 4;//yeni ticket çarpanı
uint8_t TICKET_MIN = 0;//minimum ticket sayısı
uint64_t TICKET_DEF = MULTIPLIER_OLD*PRI_DEF;//default ticket sayısı
uint64_t TICKET_MAX = MULTIPLIER_OLD*PRI_MAX;//max ticket sayısı
    
    
  
/*------------------thread.c------------------------*/
/*
* verilen threade verillen sayıda ticketı
* ticket numarası kullanınlmıyosa verir.
*/
void assign_tickets(struct thread *t,int numberOfTickets){
	srand(time(NULL));
	int i=0,j=0;
	for (i = 0; i < numberOfTickets; ++i)
	{
		j=rand()%TICKET_MAX;
		if(!isUsedTicket(j)){
			t->threadTickets[t->num_of_tickets]=j;
			allTids[t->num_of_tickets]=t->tid;
			t->num_of_tickets++
		}
		else 
			i--;
	}	
}
/*
* verilen ticket numarasının kullanıldığını kontrol eder
*/
bool isUsedTicket(int k){
	int i=0;
	for (i = 0; i < allUsed; ++i)
	{
		if (k == allTickets[i])
		{
			return true;
		}
	}
	return false;
}

tid_t thread_create (const char *name, int priority,
				 thread_func *function, void *aux, int tickets){
	struct thread *mThread;
	struct kernel_thread_frame *kf;
	struct switch_entry_frame *ef;
	struct switch_threads_frame *sf;
	tid_t tid;
	enum intr_level stored_level;

	ASSERT (function != NULL);

	/* Allocate thread. */
	mThread = palloc_get_page (PAL_ZERO);
	if (mThread == NULL)
	return TID_ERROR;

	/* Initialize thread. */
	init_thread (mThread, name, priority, tickets);
	//sistem max ticketa ulastıysa yeni ticketlar üretir
	if (((TICKET_MAX - allUsed) < tickets) || (TICKET_MAX == allUsed))//hw1b
	{		
		MULTIPLIER_NEW = MULTIPLIER_OLD + 1;
		MULTIPLIER_OLD = MULTIPLIER_NEW;
		TICKET_DEF = MULTIPLIER_NEW*PRI_DEF;
		TICKET_MAX = MULTIPLIER_NEW*PRI_MAX;


		allTickets=(int*) realloc(allTickets,TICKET_MAX*sizeof(int));
		allTids=(tid_t*) realloc(allTids,TICKET_MAX*sizeof(tid_t));
		mThread->threadTickets=(int*) realloc(mThread->threadTickets,TICKET_MAX*sizeof(int));

	}else{
		allUsed += tickets;
	}
	mThread->num_of_tickets = tickets;//hw1b
	mThread-threadTickets=(int*) malloc(TICKET_MAX*sizeof(int));
	assign_tickets(mThread,tickets);

	tid = mThread->tid = allocate_tid ();

	/* Prepare thread for first run by initializing its stack.
	Do this atomically so intermediate values for the 'stack'
	member cannot be observed. */
	stored_level = intr_disable ();

	/* Stack frame for kernel_thread(). */
	kf = alloc_frame (mThread, sizeof *kf);
	kf->eip = NULL;
	kf->function = function;
	kf->aux = aux;

	/* Stack frame for switch_entry(). */
	ef = alloc_frame (mThread, sizeof *ef);
	ef->eip = (void (*) (void)) kernel_thread;

	/* Stack frame for switch_threads(). */
	sf = alloc_frame (mThread, sizeof *sf);
	sf->eip = switch_entry;
	sf->ebp = 0;

	intr_set_level (stored_level);

	/* Add to run queue. */
	thread_unblock (mThread);

	stored_level = intr_disable ();
	/*Eger eklenen thread 'in current threadten daha buyuk priority sahipse yield edilir*/
	thread_has_max_priority();
	intr_set_level (stored_level);

	return tid;
}

/*
* thread öldüğünde elindeki ticketları sisteme bırakır
*/
void thread_exit (void)
{
	ASSERT (!intr_context ());
	int i=0;
	struct thread* mThread=thread_current();
	for (i = 0; i < allUsed; ++i)
	{
		if (mThread->threadTickets[0] == allTickets[i])
		{
			for ( ; i < (allUsed-mThread->num_of_tickets); ++i)
			{
				allTickets[i]=allTickets[i+mThread->num_of_tickets];
				allTids[i]=allTids[i+mThread->num_of_tickets];
			}
			break;
		}
	}

	allUsed -= thread_get_tickets();//biletler düşülür
	thread_set_tickets(0);//biletler iade edilir

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

static void init_thread (struct thread *t, const char *name, int priority, int tickets){
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);

	ASSERT (TICKET_MIN <= tickets && tickets <= TICKET_MAX);
	ASSERT (TICKET_USED < TICKET_MAX);
	
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->stack = (uint8_t *) t + PGSIZE;
	t->priority = priority;

	t->magic = THREAD_MAGIC;
	list_push_back (&all_list, &t->allelem);

	/*Initilization for priority donation*/
	t->initial_priority = priority;
	t->waitlock = NULL;
	list_init(&t->list_of_donation);

	/*Initilization for advanced  scheduler */
	t->nice = NICE_DEF;
	t->recent_cpu = RECENT_CPU_DEF;
}
/*
* sistem random bir sayı üretir o sayıdaki tickete sahip threadi çalıştırır
*/
static struct thread *next_thread_to_run (void){
	if (list_empty (&ready_list))
	return idle_thread;
	int random=rand%allUsed;
	int i=0;

	struct list_elem* curr= list_next (&ready_list);
	for (int i = 0; i < allUsed; ++i)
	{		
		struct list_elem* next= curr->next;
		struct thread *thread1 = list_entry(curr, struct thread, elem);
		if (thread1->tid == allTids[random])
		{
			return list_entry(thread1, struct thread, elem);
		}else{
			curr=next;
		}
	}
	return list_entry(list_next (&ready_list), struct thread, elem);
}


int thread_get_tickets(void){
	struct thread *cur = running_thread ();
	return thread_cur -> num_of_tickets;
}
void thread_set_tickets(int new_tickets){
	if (!thread_mlfqs)	{
		uint8_t stored_level = intr_disable ();
		int stored_tickets = thread_current()->num_of_tickets;
		thread_current ()->num_of_tickets = new_tickets;
		
		// yeni ticketsayisi buyuk ise donate edilir
		if (stored_tickets < thread_current()->num_of_tickets)
			lottery_ticket_donation();		
		
		if (stored_tickets > thread_current()->num_of_tickets)
			thread_has_max_priority();
		intr_set_level (stored_level);
	}
}

bool compare_tickets(const struct list_elem *first,const struct list_elem *second){
	struct thread *thread1 = list_entry(first, struct thread, elem);
	struct thread *thread2 = list_entry(second, struct thread, elem);
	
	if (thread1->num_of_tickets <= thread2->num_of_tickets)
		return false;
	
	return true;
}


/* Prioritisi yüksek bir thread prioritisi düşük bir threadin elindeki bir kaynağı beklediğinde 
* prioritesi yüksek thread elindeki biletleri düşük prioriteli threade verip 
* düşük prioritesi olan threadin çalıştırılma ve işini bitirip elindeki kaynağı 
* bırakma süresini/ihtimalini yükseltmesi gerekiyor.
 */
void lottery_ticket_donation (void){
	int i = 0;
	struct thread *mThread = thread_current();
	struct lock *mLock = mThread->waitlock;
	if (mLock)
	{
		int numOfTicketCurrent=mThread->num_of_tickets;
		int numOfTicketHolder=mLock->holder->num_of_tickets;
		/* lock tutulmuyorsa donationa gerek yok return edilir */
		if ((!mLock->holder) || ( numOfTicketHolder>= numOfTicketCurrent))
			return;

		mLock->holder->priority = mThread->priority;

		for (i = 0; i < numOfTicketHolder; ++i)
		{
			mThread->threadTickets[currentUsed]=mLock->holder->threadTickets[i];
			int j=0;
			for (j = 0; j < allUsed; ++j)
			{
				if (mLock->holder->tid == allTids[j])
				{
					int k=0;
					for (k = j; k < numOfTicketHolder; ++k)
					{
						allTids[k]=mThread->tid;
					}
				}
			}
		}
	}
}