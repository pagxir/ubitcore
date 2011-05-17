#include <stdio.h>
#include <assert.h>
#include <winsock.h>

#include "timer.h"
#include "slotwait.h"
#include "slotsock.h"

static int _wait_rescan = 0;
static int _requst_quited = 0;
static struct waitcb * _ready_header = 0;
static struct waitcb ** _ready_tailer = &_ready_header;

void waitcb_init(struct waitcb * waitcbp, wait_call * call, void * udata)
{
	waitcbp->wt_udata = udata;
	waitcbp->wt_callback = call;
	waitcbp->wt_next = 0;
	waitcbp->wt_prev = &waitcbp->wt_next;
	waitcbp->wt_magic = WT_MAGIC;
	waitcbp->wt_flags = (WT_INACTIVE| WT_EXTERNAL);
}

void waitcb_switch(struct waitcb * waitcbp)
{
	int flags;
	assert(waitcbp->wt_magic == WT_MAGIC);
	assert(waitcbp->wt_flags & WT_INACTIVE);

	flags = waitcbp->wt_flags;
	if (flags & WT_EXTERNAL) 
		_wait_rescan = 1;

	*_ready_tailer = waitcbp;
	waitcbp->wt_next = 0;
	waitcbp->wt_prev = _ready_tailer;
	waitcbp->wt_flags &= ~WT_INACTIVE;
	_ready_tailer = &waitcbp->wt_next;
}

void waitcb_cancel(struct waitcb * waitcbp)
{
	assert(waitcbp->wt_magic == WT_MAGIC);

	if (waitcb_active(waitcbp)) {
		*waitcbp->wt_prev = waitcbp->wt_next;
		if (waitcbp->wt_next != NULL) {
			assert(_ready_tailer != &waitcbp->wt_next);
			waitcbp->wt_next->wt_prev = waitcbp->wt_prev;
		} else if (_ready_tailer == &waitcbp->wt_next) {
			/* +__+__+__+__+__+__ */
			_ready_tailer = waitcbp->wt_prev;
		}
		waitcbp->wt_prev = &waitcbp->wt_next;
		waitcbp->wt_flags |= WT_INACTIVE;
	}
}

void waitcb_clean(struct waitcb * waitcbp)
{
	waitcb_cancel(waitcbp);
	waitcbp->wt_magic = 0;
	waitcbp->wt_flags = 0;
	waitcbp->wt_udata  = 0;
	waitcbp->wt_callback  = 0;
}

int waitcb_completed(struct waitcb * waitcbp)
{
	int flags;
	flags = waitcbp->wt_flags;
	assert(waitcbp->wt_magic == WT_MAGIC);
   	return (flags & WT_COMPLETE);
}

int waitcb_active(struct waitcb * waitcbp)
{
	int wait_flags;
	wait_flags = waitcbp->wt_flags;
   	wait_flags &= WT_INACTIVE;
	assert(waitcbp->wt_magic == WT_MAGIC);
	return (wait_flags == 0);
}

int waitcb_clear(struct waitcb * waitcbp)
{
	int flags;
	flags = waitcbp->wt_flags;
	waitcbp->wt_flags &= ~WT_COMPLETE;
	assert(waitcbp->wt_magic == WT_MAGIC);
	return (flags & WT_COMPLETE);
}

void slot_insert(slotcb * slotcbp, struct waitcb * waitcbp)
{
	assert(waitcbp->wt_magic == WT_MAGIC);
	assert(waitcbp->wt_flags & WT_INACTIVE);

	waitcbp->wt_flags &= ~WT_INACTIVE;
	waitcbp->wt_next = *slotcbp;
	if (*slotcbp != NULL)
		(*slotcbp)->wt_prev = &waitcbp->wt_next;
	waitcbp->wt_prev = slotcbp;
	*slotcbp = waitcbp;
}

void slot_wakeup(slotcb * slotcbp)
{
	struct waitcb * waitcbp;

	while (*slotcbp != NULL) {
		waitcbp = *slotcbp;
		waitcb_cancel(waitcbp);
		waitcb_switch(waitcbp);
	}

	return;
}

int slot_wait(wait_call ** callbackp, void ** udatap)
{
	struct waitcb mark;
	struct waitcb * waitcbp;

	waitcb_init(&mark, 0, 0);
	mark.wt_flags &= ~WT_EXTERNAL;

	_wait_rescan = 0;
	waitcb_switch(&mark);
	for ( ; ; ) {
		waitcbp = _ready_header;
		waitcb_cancel(waitcbp);
		if (waitcbp == &mark) {
			DWORD timeout = 20;
			if (_requst_quited) {
			   	_wait_rescan = 0;
				return 0;
			}

			while (!_wait_rescan) {
				callout_invoke(&timeout);
				sock_selscan(&timeout);
			}
			waitcb_switch(&mark);
			_wait_rescan = 0;
			continue;
		}

		if (waitcbp->wt_flags & WT_WAITSCAN) {
		   	*callbackp = waitcbp->wt_callback;
		   	*udatap = waitcbp->wt_udata;
			waitcb_switch(waitcbp);
			(*callbackp)(*udatap);
			continue;
		}

		*callbackp = waitcbp->wt_callback;
		*udatap = waitcbp->wt_udata;
		waitcbp->wt_flags |= WT_COMPLETE;
		break;
	}

	waitcb_cancel(&mark);
	return 1;
}

int slot_fire(wait_call * call, void * udata)
{
	int error = 0;

	if (call != 0)
	   	call(udata);

	return error;
}

int slotwait_held(int held)
{
	int error = 0;

	return error;
}

int slotwait_step(void)
{
	int error;
	void * udata;
	wait_call * callback;

	error = slot_wait(&callback, &udata);
	if (error == 1)
	   	(void)slot_fire(callback, udata);

	return error;
}

static struct waitcb * _start_slot = 0;
void slotwait_start(void)
{
	slot_wakeup(&_start_slot);
	_requst_quited = 0;
	return;
}

void slotwait_atstart(struct waitcb * waitcbp)
{
	slot_insert(&_start_slot, waitcbp);
	return;
}

static struct waitcb * _stop_slot = 0;
void slotwait_stop(void)
{
	slot_wakeup(&_stop_slot);
	_requst_quited = 1;
	return;
}

void slotwait_atstop(struct waitcb * waitcbp)
{
	slot_insert(&_stop_slot, waitcbp);
	return;
}

