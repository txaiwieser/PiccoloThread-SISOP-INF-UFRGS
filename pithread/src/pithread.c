/*
 * pithread.c
 *
 *
 *
 */

#define _XOPEN_SOURCE 600 // Solves a OSX deprecated library problem of ucontext.h
#include <ucontext.h>

#include "../include/pithread.h"
#include "../include/pidata.h"
#include "pidata.c"
#include "scheduler_functions.c"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


/*----------------------------------------------------------------------------*/
/*                                    DATA                                    */
/*----------------------------------------------------------------------------*/
int global_var_tid = 0;
ucontext_t *ending_contex = NULL;
static bool initialized = false;
/*----------------------------------------------------------------------------*/
/*                            LIB INITIALIZATION                              */
/*----------------------------------------------------------------------------*/


///
///
int internal_init(){
	if (!initialized) {
		initialized = true;
		global_var_tid = 0;
		if (init_end_thread_context() != SUCESS_CODE) {
			PIPRINT(("ERROR_CODE: init_end_thread_context"));
			return ERROR_CODE;
		} else if(init_main_thread() != SUCESS_CODE) {
			PIPRINT(("ERROR_CODE: init_main_thread"));
			return ERROR_CODE;
		} else {
			volatile bool main_thread_created = false;
			if (!main_thread_created) {
				main_thread_created = true;
				run_scheduler();
			}
            return SUCESS_CODE;
		}
	} else {
		return SUCESS_CODE;
	}
}


///
///
int init_main_thread() {
	TCB_t *thread = (TCB_t*)malloc(sizeof(TCB_t));

	thread->tid = global_var_tid;
	thread->state = PI_CREATION;
	thread->credCreate = thread->credReal = MAX_THREAD_PRIORITY;
	thread->next = NULL;
	thread->prev = NULL;
	thread->waiting_for_tid = ERROR_CODE;
	
	if (((thread->context).uc_stack.ss_sp = malloc(SIGSTKSZ)) == NULL) {
		printf("[PI ERROR]: No memory for stack allocation!");
		return ERROR_CODE;
	} else {
		(thread->context).uc_stack.ss_size = SIGSTKSZ;
		(thread->context).uc_link = NULL;
		ready_active_insert(thread);
		getcontext(&(thread->context));
		return SUCESS_CODE;
	}
}


///
///
int init_end_thread_context() {
	ending_contex = (ucontext_t *) malloc(sizeof(ucontext_t));

	if (getcontext(ending_contex) != 0 || ending_contex == NULL) {
		return ERROR_CODE;
	} else {
		ending_contex->uc_stack.ss_sp = (char *) malloc(SIGSTKSZ);
		ending_contex->uc_stack.ss_size = SIGSTKSZ;
		ending_contex->uc_link = NULL;

		makecontext(ending_contex, end_thread_execution, 0);
		return SUCESS_CODE;
	}
}


/*----------------------------------------------------------------------------*/
/*                               LIB FUNCTIONS                                */
/*----------------------------------------------------------------------------*/


///
///
int picreate(int credCreate, void* (*start)(void*), void *arg) {
	internal_init();
	
	int new_tid = ++global_var_tid;
	PIPRINT(("[PI] Creating new thread with tid: %d\n", new_tid));
	
	TCB_t *thread = (TCB_t*)malloc(sizeof(TCB_t));
	thread->tid = new_tid;
	thread->state = PI_CREATION;
	thread->credCreate = thread->credReal = credCreate;
	thread->next = NULL;thread->prev = NULL;
	thread->waiting_for_tid = ERROR_CODE;
	
	getcontext(&(thread->context));
	
	if (((thread->context).uc_stack.ss_sp = malloc(SIGSTKSZ)) == NULL) {
		printf("[PI ERROR]: No memory for stack allocation!");
		return ERROR_CODE;
	} else {
		(thread->context).uc_stack.ss_size = SIGSTKSZ;
		(thread->context).uc_link = ending_contex;
		makecontext(&(thread->context), (void (*)(void))start, 1, arg);
		ready_active_insert(thread);
		return new_tid;
	}
}

///
///
int piyield(void) {
	internal_init();

	if(!ready_active_is_empty()) {
		printf("Yield :)\n");
		return run_scheduler();
	} else {
		printf("No Yield :9\n");
		return SUCESS_CODE;
	}
}


///
///
int piwait(int tid) {
	internal_init();

	if (contains_tid_in_ready_queue(tid)) { // Thread Exist
	    current_running_thread->waiting_for_tid = tid;
	    return thread_block(current_running_thread);
	} else {
		return SUCESS_CODE;
	}
}






/*----------------------------------------------------------------------------*/
/*								     MUTEX       							  */
/*----------------------------------------------------------------------------*/

///
///
int pimutex_init(pimutex_t *mtx) {
	// Initializing mutex
	internal_init();

	if(mtx != NULL){
		mtx->flag = false;
		mtx->first = NULL;
		mtx->last = NULL;
		return SUCESS_CODE;
	} else {
		return ERROR_CODE;
	}
}


///
///
int pilock (pimutex_t *mtx) {
	internal_init();
	if(mtx != NULL){
		if (mtx->flag == MTX_LOCKED) {
			// The resouce is ALREADY being used, so we must block the thread.

			// TODO: confirm if this // will work..,
			TCB_queue_t *tempQueue = (TCB_queue_t *) malloc(sizeof(TCB_queue_t));
			tempQueue->start = mtx->first;
			tempQueue->end = mtx->last;
			queue_insert(&tempQueue, current_running_thread);
			mtx->first = tempQueue->start;
			mtx->last = tempQueue->end;
			// free(tempQueue);
			// tempQueue = NULL;
			//

			thread_block(current_running_thread);
			return SUCESS_CODE;
		} else if(mtx->flag == MTX_UNLOCKED) {
			// The resouce is NOT being used, so the thread is goint to use.
			mtx->flag = MTX_LOCKED;
			return SUCESS_CODE;
		} else {
			return ERROR_CODE;
		}
	} else {
		return ERROR_CODE;
	}
}


///
///
int piunlock (pimutex_t *mtx) {
	internal_init();
	if (mtx != NULL){
		if ((mtx->flag == MTX_LOCKED) && (mtx->first != NULL)) {
			// Mutex is locked and there is threads on the blocked queue

			// TODO: confirm if this // will work..,
			TCB_queue_t *tempQueue = (TCB_queue_t *) malloc(sizeof(TCB_queue_t));
			tempQueue->start = mtx->first;
			tempQueue->end = mtx->last;
			TCB_t *thread = queue_remove(tempQueue);
			mtx->first = tempQueue->start;
			mtx->last = tempQueue->end;
			// free(tempQueue);
			// tempQueue = NULL;
			//

			thread_unblock(thread);
		}

		if ((mtx->first == NULL) && (mtx->last == NULL)) {
			// Zero threads on the blocked queue
			mtx->flag = MTX_UNLOCKED;
		}

		return SUCESS_CODE;
	} else {
		return ERROR_CODE;
	}
}