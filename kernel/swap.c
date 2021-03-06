#include "sys.h"
#include "proc.h"
#include "buf.h"
#include "conn.h"

char memwant = NO;	/* someone wants memory, so look for a swapout */
char swapping = NO;	/* run status of swap process */

/*
 * Swap other processes between core and disk. After
 * forking the login process, the power-up process falls
 * into this code, becoming the swapping process.
 */
int
swap()
{
	fast struct proc *n;
	extern int mfree();

	swapinit();	/* Initialize the swap map */

	for (;;) {
		if (memwant) {
			if ((n = findout()) && swapout(n)) {
				mfree(n);	/* resets memwant */
			} else {
				goto sleep;
			}
		}

		if (n = findin()) {
			if (mget(n)) {
				swapin(n);
			} else {
				memwant = YES;
			}
		} else {
			sleep:	

			swapping = NO;
			sleep(&mfree, PRISWAP);
			swapping = YES;
		}
	}
}

/*
 * Find a process that needs to be swapped in.
 */
int
findin()
{
	static struct proc *p, *n;

	di();
	n = NULL;
	for (p = plist; p < plist + NPROC; p++)
		if (
		   (p->mode & (AWAKE | SWAPPED)) == (AWAKE | SWAPPED)
		   &&
			(
			n == NULL
			||
			n->time < p->time
			)
		   )
			n = p;
	ei();

	return (n);
}

/*
 * Find someone to swap out
 */
int
findout()
{
	static struct proc *p, *n;

	di();
	n = NULL;
	for (p = plist; p < plist + NPROC; p++)
		if (
		   (p->mode & (ALIVE | LOADED | LOCKED)) == (ALIVE | LOADED)
		   &&
		   p->time >= MINRUN
		   &&
		       (
		       n == NULL
		       ||
		       (n->mode & AWAKE == p->mode & AWAKE
			       &&
			n->time < p->time)
		       ||
		       n->mode & AWAKE
		       )
		   )
			n = p;
	ei();

	return (n);
}

/*
 * Swap out process p. Called from swap() above and from fork().
 */
#ifdef KNR
swapout(p)
	fast struct proc *p;
#else
int
swapout(struct proc *p)
#endif
{
	p->mode &= ~LOADED;

	if ((p->swap = salloc(p->nsegs << 3)) != 0 && swapio(BWRITE, p)) {
		p->mode |= SWAPPED;
		return YES;
	} else {
		p->mode |= LOADED;
		send(p, SIGKILL);
		pr("swap error: process %i killed\n", procid(p));
		return NO;
	}
}

/*
 * Swap in process p.
 */
#ifdef KNR
swapin(p)
	fast struct proc *p;
#else
int
swapin(struct proc *p)
#endif
{
	p->mode &= ~SWAPPED;

	if (swapio(BREAD, p)) {
		sfree(p->nsegs << 3, p->swap);
		p->mode |= LOADED;
		return YES;
	} else {
		mfree(p);
		pr("swap error: process %i hung\n", procid(p));
		return NO;
	}
}

/*
 * Buffer headers for swap io. These are passed to the swap
 * device strategy routine to read or write a large
 * core image at once without impacting regular buffers.
 */
struct buf swab[17] = 0,
	       lock = 0;

/*
 * Read or write core to swap space.
 */
#ifdef KNR
swapio(flag, p)
	int flag;
	fast struct proc *p;
#else
int 
swapio(int flag, struct proc *p)
#endif
{
	static UCHAR i, seg, success;
	static UINT blk;
	static struct buf *b;

	until (block(&lock))
		;
	blk = p->swap;
	for (i = 0; i < 17; i++) {
		if (p->mem[i].per != FULL)
			continue;
		seg = p->mem[i].seg;
		b = &swab[i];
		b->flags &= ~(BREAD | BDONE | BSYNC | BERROR);
		b->flags |= flag | (BLOCK | BBUSY);
		b->dev	 = swapdev;
		b->blk	 = blk;
		b->count = 4096;
		b->data	 = (seg & 15) << 12;
		b->xmem	 = seg >> 4;
		b->error = 0;
		(* biosw[major(swapdev)].strat)(b);
		blk += 8;
	}

	success = YES;
	for (i = 0; i < 17; i++)
		if (p->mem[i].per == FULL) {
			b = &swab[i];
			bwait(b);
			if (b->flags & BERROR) {
				if (b->error == ENXIO)
					pr("Swap space full\n");
				success = NO;
			}
		}
	brelse(&lock);
	p->time = 0;
	return (success);
}
