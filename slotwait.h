#ifndef _SLOTWAIT_H_
#define _SLOTWAIT_H_

typedef struct waitcb *slotcb;
typedef void wait_call(void *data);

struct waitcb {
	int wt_magic;
	int wt_flags;
	int wt_count;
	struct waitcb *wt_next;
	struct waitcb **wt_prev;

	void *wt_data;
	size_t wt_value;
	
	void *wt_udata;
	wait_call *wt_callback;
};

void waitcb_init(struct waitcb *wait, wait_call *call, void *udata);
void waitcb_switch(struct waitcb *wait);
void waitcb_cancel(struct waitcb *wait);
void waitcb_clean(struct waitcb *wait);

int waitcb_completed(struct waitcb *wait);
int waitcb_active(struct waitcb *wait);
int waitcb_clear(struct waitcb *wait);

void slot_record(slotcb *slot, struct waitcb *cb);
void slot_wakeup(slotcb *slot);

int slot_wait(wait_call **callp, void **udatap);
int slot_fire(wait_call *call, void *udata);

int slotwait_held(int hold);
int slotwait_step(void);

void slotwait_atstart(struct waitcb *waitcbp);
void slotwait_atstop(struct waitcb *waitcbp);
void slotwait_start(void);
void slotwait_stop(void);

HANDLE slotwait_handle(void);

#define WT_MAGIC    0x19821130
#define WT_EXTERNAL 0x00000001
#define WT_INACTIVE 0x00000002
#define WT_COMPLETE 0x00000004
#define WT_WAITSCAN 0x00000008

#endif

