/* $Id: jbuf.c 3111 2010-02-25 12:07:29Z nanang $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
/*
 * Based on implementation kindly contributed by Switchlab, Ltd.
 */
#include <pjmedia/jbuf.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/string.h>


#define THIS_FILE   "jbuf.c"


/* Minimal difference between JB size and 2*burst-level to perform 
 * JB shrinking. 
 */
#define SAFE_SHRINKING_DIFF	1

/* Minimal gap (in ms) between JB shrinking */
#define MIN_SHRINK_GAP_MSEC	200

/* Invalid sequence number, used as the initial value. */
#define INVALID_OFFSET		-9999

/* Maximum burst length, whenever an operation is bursting longer than 
 * this value, JB will assume that the opposite operation was idle.
 */
#define MAX_BURST_MSEC		1000

/* Number of OP switches to be performed in JB_STATUS_INITIALIZING, before
 * JB can switch its states to JB_STATUS_PROCESSING.
 */
#define INIT_CYCLE		10


/* Struct of JB internal buffer, represented in a circular buffer containing
 * frame content, frame type, frame length, and frame bit info.
 */
typedef struct jb_framelist_t
{
    /* Settings */
    unsigned	     frame_size;	/**< maximum size of frame	    */
    unsigned	     max_count;		/**< maximum number of frames	    */

    /* Buffers */
    char	    *content;		/**< frame content array	    */
    int		    *frame_type;	/**< frame type array		    */
    pj_size_t	    *content_len;	/**< frame length array		    */
    pj_uint32_t	    *bit_info;		/**< frame bit info array	    */
    
    /* States */
    unsigned	     head;		/**< index of head, pointed frame
					     will be returned by next GET   */
    unsigned	     size;		/**< current size of framelist, 
					     including discarded frames.    */
    unsigned	     discarded_num;	/**< current number of discarded 
					     frames.			    */
    int		     origin;		/**< original index of flist_head   */

} jb_framelist_t;


struct pjmedia_jbuf
{
    /* Settings (consts) */
    pj_str_t	    jb_name;		/**< jitter buffer name		    */
    pj_size_t	    jb_frame_size;	/**< frame size			    */
    unsigned	    jb_frame_ptime;	/**< frame duration.		    */
    pj_size_t	    jb_max_count;	/**< capacity of jitter buffer, 
					     in frames			    */
    int		    jb_init_prefetch;	/**< Initial prefetch		    */
    int		    jb_min_prefetch;	/**< Minimum allowable prefetch	    */
    int		    jb_max_prefetch;	/**< Maximum allowable prefetch	    */
    int		    jb_max_burst;	/**< maximum possible burst, whenever
					     burst exceeds this value, it 
					     won't be included in level 
					     calculation		    */
    int		    jb_min_shrink_gap;	/**< How often can we shrink	    */

    /* Buffer */
    jb_framelist_t  jb_framelist;	/**< the buffer			    */

    /* States */
    int		    jb_level;		/**< delay between source & 
					     destination (calculated according 
					     of the number of burst get/put 
					     operations)		    */
    int		    jb_max_hist_level;  /**< max level during the last level 
					     calculations		    */
    int		    jb_stable_hist;	/**< num of times the delay has	been 
					     lower then the prefetch num    */
    int		    jb_last_op;		/**< last operation executed 
					     (put/get)			    */
    int		    jb_eff_level;	/**< effective burst level	    */
    int		    jb_prefetch;	/**< no. of frame to insert before 
					     removing some (at the beginning 
					     of the framelist->content 
					     operation), the value may be
					     continuously updated based on
					     current frame burst level.	    */
    int		    jb_status;		/**< status is 'init' until the	first 
					     'put' operation		    */
    int		    jb_init_cycle_cnt;	/**< status is 'init' until the	first 
					     'put' operation		    */
    int		    jb_last_del_seq;	/**< Seq # of last frame deleted    */

    int		    jb_last_discard_seq;/**< Seq # of last frame discarded  */

    /* Statistics */
    pj_math_stat    jb_delay;		/**< Delay statistics of jitter buffer 
					     (in ms)			    */
    pj_math_stat    jb_burst;		/**< Burst statistics (in frames)   */
    unsigned	    jb_lost;		/**< Number of lost frames.	    */
    unsigned	    jb_discard;		/**< Number of discarded frames.    */
    unsigned	    jb_empty;		/**< Number of empty/prefetching frame
					     returned by GET. */
};


#define JB_STATUS_INITIALIZING	0
#define JB_STATUS_PROCESSING	1
#define JB_STATUS_PREFETCHING	2



/* Progressive discard algorithm introduced to reduce JB latency
 * by discarding incoming frames with adaptive aggressiveness based on
 * actual burst level.
 */
#define PROGRESSIVE_DISCARD 1

/* Internal JB frame flag, discarded frame will not be returned by JB to
 * application, it's just simply discarded.
 */
#define PJMEDIA_JB_DISCARDED_FRAME 1024



/* Enabling this would log the jitter buffer state about once per 
 * second.
 */
#if 1
#  define TRACE__(args)	    PJ_LOG(5,args)
#else
#  define TRACE__(args)
#endif

static pj_status_t jb_framelist_reset(jb_framelist_t *framelist);
static unsigned jb_framelist_remove_head(jb_framelist_t *framelist,
					 unsigned count);

static pj_status_t jb_framelist_init( pj_pool_t *pool,
				      jb_framelist_t *framelist,
				      unsigned frame_size,
				      unsigned max_count) 
{
    PJ_ASSERT_RETURN(pool && framelist, PJ_EINVAL);

    pj_bzero(framelist, sizeof(jb_framelist_t));

    framelist->frame_size   = frame_size;
    framelist->max_count    = max_count;
    framelist->content	    = (char*) 
			      pj_pool_alloc(pool,
					    framelist->frame_size* 
					    framelist->max_count);
    framelist->frame_type   = (int*)
			      pj_pool_alloc(pool, 
					    sizeof(framelist->frame_type[0])*
					    framelist->max_count);
    framelist->content_len  = (pj_size_t*)
			      pj_pool_alloc(pool, 
					    sizeof(framelist->content_len[0])*
					    framelist->max_count);
    framelist->bit_info	    = (pj_uint32_t*)
			      pj_pool_alloc(pool, 
					    sizeof(framelist->bit_info[0])*
					    framelist->max_count);


    return jb_framelist_reset(framelist);

}

static pj_status_t jb_framelist_destroy(jb_framelist_t *framelist) 
{
    PJ_UNUSED_ARG(framelist);
    return PJ_SUCCESS;
}

static pj_status_t jb_framelist_reset(jb_framelist_t *framelist) 
{
    framelist->head = 0;
    framelist->origin = INVALID_OFFSET;
    framelist->size = 0;
    framelist->discarded_num = 0;


    //pj_bzero(framelist->content, 
    //	     framelist->frame_size * 
    //	     framelist->max_count);

    pj_memset(framelist->frame_type,
	      PJMEDIA_JB_MISSING_FRAME,
	      sizeof(framelist->frame_type[0]) * 
	      framelist->max_count);

    pj_bzero(framelist->content_len, 
	     sizeof(framelist->content_len[0]) * 
	     framelist->max_count);

    //pj_bzero(framelist->bit_info,
    //	     sizeof(framelist->bit_info[0]) * 
    //	     framelist->max_count);

    return PJ_SUCCESS;
}


static unsigned jb_framelist_size(const jb_framelist_t *framelist) 
{
    return framelist->size;
}


static unsigned jb_framelist_eff_size(const jb_framelist_t *framelist) 
{
    return (framelist->size - framelist->discarded_num);
}

static int jb_framelist_origin(const jb_framelist_t *framelist) 
{
    return framelist->origin;
}


static pj_bool_t jb_framelist_get(jb_framelist_t *framelist,
				  void *frame, pj_size_t *size,
				  pjmedia_jb_frame_type *p_type,
				  pj_uint32_t *bit_info) 
{
    if (framelist->size) {

	/* Skip discarded frames */
	while (framelist->frame_type[framelist->head] ==
	       PJMEDIA_JB_DISCARDED_FRAME)
	{
	    jb_framelist_remove_head(framelist, 1);
	}

	/* Return the head frame if any */
	if (framelist->size) {
	    pj_memcpy(frame, 
		      framelist->content + 
		      framelist->head * framelist->frame_size,
		      framelist->frame_size);
	    *p_type = (pjmedia_jb_frame_type) 
		      framelist->frame_type[framelist->head];
	    if (size)
		*size   = framelist->content_len[framelist->head];
	    if (bit_info)
		*bit_info = framelist->bit_info[framelist->head];

	    //pj_bzero(framelist->content + 
	    //	 framelist->head * framelist->frame_size,
	    //	 framelist->frame_size);
	    framelist->frame_type[framelist->head] = PJMEDIA_JB_MISSING_FRAME;
	    framelist->content_len[framelist->head] = 0;
	    framelist->bit_info[framelist->head] = 0;

	    framelist->origin++;
	    framelist->head = (framelist->head + 1) % framelist->max_count;
	    framelist->size--;
    	
	    return PJ_TRUE;
	}
    }

    /* No frame available */
    pj_bzero(frame, framelist->frame_size);

    return PJ_FALSE;
}


/* Remove oldest frames as many as param 'count' */
static unsigned jb_framelist_remove_head(jb_framelist_t *framelist,
					 unsigned count) 
{
    if (count > framelist->size) 
	count = framelist->size;

    if (count) {
	/* may be done in two steps if overlapping */
	unsigned step1,step2;
	unsigned tmp = framelist->head+count;
	unsigned i;

	if (tmp > framelist->max_count) {
	    step1 = framelist->max_count - framelist->head;
	    step2 = count-step1;
	} else {
	    step1 = count;
	    step2 = 0;
	}

	for (i = framelist->head; i < (framelist->head + step1); ++i) {
	    if (framelist->frame_type[i] == PJMEDIA_JB_DISCARDED_FRAME) {
		pj_assert(framelist->discarded_num > 0);
		framelist->discarded_num--;
	    }
	}

	//pj_bzero(framelist->content + 
	//	    framelist->head * framelist->frame_size,
	//          step1*framelist->frame_size);
	pj_memset(framelist->frame_type+framelist->head,
		  PJMEDIA_JB_MISSING_FRAME,
		  step1*sizeof(framelist->frame_type[0]));
	pj_bzero(framelist->content_len+framelist->head,
		 step1*sizeof(framelist->content_len[0]));

	if (step2) {
	    for (i = 0; i < step2; ++i) {
		if (framelist->frame_type[i] == PJMEDIA_JB_DISCARDED_FRAME) {
		    pj_assert(framelist->discarded_num > 0);
		    framelist->discarded_num--;
		}
	    }
	    //pj_bzero( framelist->content,
	    //	      step2*framelist->frame_size);
	    pj_memset(framelist->frame_type,
		      PJMEDIA_JB_MISSING_FRAME,
		      step2*sizeof(framelist->frame_type[0]));
	    pj_bzero (framelist->content_len,
		      step2*sizeof(framelist->content_len[0]));
	}

	/* update states */
	framelist->origin += count;
	framelist->head = (framelist->head + count) % framelist->max_count;
	framelist->size -= count;
    }
    
    return count;
}


static pj_status_t jb_framelist_put_at(jb_framelist_t *framelist,
				       int index,
				       const void *frame,
				       unsigned frame_size,
				       pj_uint32_t bit_info,
				       unsigned frame_type)
{
    int distance;
    unsigned pos;
    enum { MAX_MISORDER = 100 };
    enum { MAX_DROPOUT = 3000 };

    assert(frame_size <= framelist->frame_size);

    /* too late or sequence restart */
    if (index < framelist->origin) {
	if (framelist->origin - index < MAX_MISORDER) {
	    /* too late */
	    return PJ_ETOOSMALL;
	} else {
	    /* sequence restart */
	    framelist->origin = index - framelist->size;
	}
    }

    /* if jbuf is empty, just reset the origin */
    if (framelist->size == 0) {
	pj_assert(framelist->discarded_num == 0);
	framelist->origin = index;
    }

    /* get distance of this frame to the first frame in the buffer */
    distance = index - framelist->origin;

    /* far jump, the distance is greater than buffer capacity */
    if (distance >= (int)framelist->max_count) {
	if (distance > MAX_DROPOUT) {
	    /* jump too far, reset the buffer */
	    jb_framelist_reset(framelist);
	    framelist->origin = index;
	    distance = 0;
	} else {
	    /* otherwise, reject the frame */
	    return PJ_ETOOMANY;
	}
    }

    /* get the slot position */
    pos = (framelist->head + distance) % framelist->max_count;

    /* if the slot is occupied, it must be duplicated frame, ignore it. */
    if (framelist->frame_type[pos] != PJMEDIA_JB_MISSING_FRAME)
	return PJ_EEXISTS;

    /* put the frame into the slot */
    framelist->frame_type[pos] = frame_type;
    framelist->content_len[pos] = frame_size;
    framelist->bit_info[pos] = bit_info;

    /* update framelist size */
    if (framelist->origin + (int)framelist->size <= index)
	framelist->size = distance + 1;

    if(PJMEDIA_JB_NORMAL_FRAME == frame_type) {
	/* copy frame content */
	pj_memcpy(framelist->content + pos * framelist->frame_size,
		  frame, frame_size);
	return PJ_SUCCESS;
    } else {
	/* frame is being discarded */
	framelist->discarded_num++;
	return PJ_EIGNORED;
    }
}



enum pjmedia_jb_op
{
    JB_OP_INIT  = -1,
    JB_OP_PUT   = 1,
    JB_OP_GET   = 2
};


PJ_DEF(pj_status_t) pjmedia_jbuf_create(pj_pool_t *pool, 
					const pj_str_t *name,
					unsigned frame_size, 
					unsigned ptime,
					unsigned max_count,
					pjmedia_jbuf **p_jb)
{
    pjmedia_jbuf *jb;
    pj_status_t status;

    jb = PJ_POOL_ZALLOC_T(pool, pjmedia_jbuf);

    status = jb_framelist_init(pool, &jb->jb_framelist, frame_size, max_count);
    if (status != PJ_SUCCESS)
	return status;

    pj_strdup_with_null(pool, &jb->jb_name, name);
    jb->jb_frame_size	 = frame_size;
    jb->jb_frame_ptime   = ptime;
    jb->jb_prefetch	 = PJ_MIN(PJMEDIA_JB_DEFAULT_INIT_DELAY,max_count*4/5);
    jb->jb_min_prefetch  = 0;
    jb->jb_max_prefetch  = max_count*4/5;
    jb->jb_max_count	 = max_count;
    jb->jb_min_shrink_gap= MIN_SHRINK_GAP_MSEC / ptime;
    jb->jb_max_burst	 = MAX_BURST_MSEC / ptime;
    jb->jb_last_discard_seq = 0;

    pj_math_stat_init(&jb->jb_delay);
    pj_math_stat_init(&jb->jb_burst);

    pjmedia_jbuf_reset(jb);

    *p_jb = jb;
    return PJ_SUCCESS;
}


/*
 * Set the jitter buffer to fixed delay mode. The default behavior
 * is to adapt the delay with actual packet delay.
 *
 */
PJ_DEF(pj_status_t) pjmedia_jbuf_set_fixed( pjmedia_jbuf *jb,
					    unsigned prefetch)
{
    PJ_ASSERT_RETURN(jb, PJ_EINVAL);
    PJ_ASSERT_RETURN(prefetch <= jb->jb_max_count, PJ_EINVAL);

    jb->jb_min_prefetch = jb->jb_max_prefetch = 
	jb->jb_prefetch = jb->jb_init_prefetch = prefetch;

    return PJ_SUCCESS;
}


/*
 * Set the jitter buffer to adaptive mode.
 */
PJ_DEF(pj_status_t) pjmedia_jbuf_set_adaptive( pjmedia_jbuf *jb,
					       unsigned prefetch,
					       unsigned min_prefetch,
					       unsigned max_prefetch)
{
    PJ_ASSERT_RETURN(jb, PJ_EINVAL);
    PJ_ASSERT_RETURN(min_prefetch < max_prefetch &&
		     prefetch <= max_prefetch &&
		     max_prefetch <= jb->jb_max_count,
		     PJ_EINVAL);

    jb->jb_prefetch = jb->jb_init_prefetch = prefetch;
    jb->jb_min_prefetch = min_prefetch;
    jb->jb_max_prefetch = max_prefetch;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_jbuf_reset(pjmedia_jbuf *jb)
{
    jb->jb_level	 = 0;
    jb->jb_last_op	 = JB_OP_INIT;
    jb->jb_stable_hist	 = 0;
    jb->jb_status	 = JB_STATUS_INITIALIZING;
    jb->jb_init_cycle_cnt= 0;
    jb->jb_max_hist_level= 0;

    jb_framelist_reset(&jb->jb_framelist);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_jbuf_destroy(pjmedia_jbuf *jb)
{
    PJ_LOG(5, (jb->jb_name.ptr, ""
	       "JB summary:\n"
	       "  size=%d prefetch=%d level=%d\n"
	       "  delay (min/max/avg/dev)=%d/%d/%d/%d ms\n"
	       "  burst (min/max/avg/dev)=%d/%d/%d/%d frames\n"
	       "  lost=%d discard=%d empty=%d",
	       jb->jb_framelist.size, jb->jb_prefetch, jb->jb_eff_level,
	       jb->jb_delay.min, jb->jb_delay.max, jb->jb_delay.mean, 
	       pj_math_stat_get_stddev(&jb->jb_delay),
	       jb->jb_burst.min, jb->jb_burst.max, jb->jb_burst.mean, 
	       pj_math_stat_get_stddev(&jb->jb_burst),
	       jb->jb_lost, jb->jb_discard, jb->jb_empty));

    return jb_framelist_destroy(&jb->jb_framelist);
}


static void jbuf_calculate_jitter(pjmedia_jbuf *jb)
{
    int diff, cur_size;

    cur_size = jb_framelist_eff_size(&jb->jb_framelist);
    pj_math_stat_update(&jb->jb_burst, jb->jb_level);
    jb->jb_max_hist_level = PJ_MAX(jb->jb_max_hist_level, jb->jb_level);

    /* Burst level is decreasing */
    if (jb->jb_level < jb->jb_eff_level) {

	enum { STABLE_HISTORY_LIMIT = 20 };
        
	jb->jb_stable_hist++;
        
	/* Only update the effective level (and prefetch) if 'stable' 
	 * condition is reached (not just short time impulse)
	 */
	if (jb->jb_stable_hist > STABLE_HISTORY_LIMIT) {
    	
	    diff = (jb->jb_eff_level - jb->jb_max_hist_level) / 3;

	    if (diff < 1)
		diff = 1;

	    /* Update effective burst level */
	    jb->jb_eff_level -= diff;

	    /* Update prefetch based on level */
	    if (jb->jb_init_prefetch) {
		jb->jb_prefetch = jb->jb_eff_level;
		if (jb->jb_prefetch < jb->jb_min_prefetch) 
		    jb->jb_prefetch = jb->jb_min_prefetch;
	    }

	    /* Reset history */
	    jb->jb_max_hist_level = 0;
	    jb->jb_stable_hist = 0;

	    TRACE__((jb->jb_name.ptr,"jb updated(1), lvl=%d pre=%d, size=%d",
		     jb->jb_eff_level, jb->jb_prefetch, cur_size));
	}
    }

    /* Burst level is increasing */
    else if (jb->jb_level > jb->jb_eff_level) {

	/* Instaneous set effective burst level to recent maximum level */
	jb->jb_eff_level = PJ_MIN(jb->jb_max_hist_level,
				  (int)(jb->jb_max_count*4/5));

	/* Update prefetch based on level */
	if (jb->jb_init_prefetch) {
	    jb->jb_prefetch = jb->jb_eff_level;
	    if (jb->jb_prefetch > jb->jb_max_prefetch)
		jb->jb_prefetch = jb->jb_max_prefetch;
	}

	jb->jb_stable_hist = 0;
	/* Do not reset max_hist_level. */
	//jb->jb_max_hist_level = 0;

	TRACE__((jb->jb_name.ptr,"jb updated(2), lvl=%d pre=%d, size=%d", 
		 jb->jb_eff_level, jb->jb_prefetch, cur_size));
    }

    /* Level is unchanged */
    else {
	jb->jb_stable_hist = 0;
    }
}

PJ_INLINE(void) jbuf_update(pjmedia_jbuf *jb, int oper)
{
    if(jb->jb_last_op != oper) {
	jb->jb_last_op = oper;

	if (jb->jb_status == JB_STATUS_INITIALIZING) {
	    /* Switch status 'initializing' -> 'processing' after some OP 
	     * switch cycles and current OP is GET (burst level is calculated 
	     * based on PUT burst), so burst calculation is guaranted to be
	     * performed right after the status switching.
	     */
	    if (++jb->jb_init_cycle_cnt >= INIT_CYCLE && oper == JB_OP_GET) {
		jb->jb_status = JB_STATUS_PROCESSING;
	    } else {
		jb->jb_level = 0;
		return;
	    }
	}

	/* Perform jitter calculation based on PUT burst-level only, since 
	 * GET burst-level may not be accurate, e.g: when VAD is active.
	 * Note that when burst-level is too big, i.e: exceeds jb_max_burst,
	 * the GET op may be idle, in this case, we better skip the jitter 
	 * calculation.
	 */
	if (oper == JB_OP_GET && jb->jb_level < jb->jb_max_burst)
	    jbuf_calculate_jitter(jb);

	jb->jb_level = 0;
    }

    /* These code is used for shortening the delay in the jitter buffer.
     * It needs shrink only when there is possibility of drift. Drift
     * detection is performed by inspecting the jitter buffer size, if
     * its size is twice of current burst level, there can be drift.
     *
     * Moreover, normally drift level is quite low, so JB shouldn't need
     * to shrink aggresively, it will shrink maximum one frame per 
     * MIN_SHRINK_GAP_MSEC ms. Theoritically, JB may handle drift level 
     * as much as = FRAME_PTIME/MIN_SHRINK_GAP_MSEC * 100%
     *
     * Whenever there is drift, where PUT > GET, this method will keep 
     * the latency (JB size) as much as twice of burst level.
     */

    /* Shrinking due of drift will be implicitly done by progressive discard,
     * so just disable it when progressive discard is active.
     */
#if !PROGRESSIVE_DISCARD

    if (jb->jb_status != JB_STATUS_PROCESSING)
	return;

    {
	int diff, burst_level;

	burst_level = PJ_MAX(jb->jb_eff_level, jb->jb_level);
	diff = jb_framelist_eff_size(&jb->jb_framelist) - burst_level*2;

	if (diff >= SAFE_SHRINKING_DIFF) {
	    int seq_origin;

	    /* Check and adjust jb_last_del_seq, in case there was 
	     * seq restart 
	     */
	    seq_origin = jb_framelist_origin(&jb->jb_framelist);
	    if (seq_origin < jb->jb_last_del_seq)
		jb->jb_last_del_seq = seq_origin;

	    if (seq_origin - jb->jb_last_del_seq >= jb->jb_min_shrink_gap)
	    {
		/* Shrink slowly, one frame per cycle */
		diff = 1;

		/* Drop frame(s)! */
		diff = jb_framelist_remove_head(&jb->jb_framelist, diff);
		jb->jb_last_del_seq = jb_framelist_origin(&jb->jb_framelist);
		jb->jb_discard += diff;

		TRACE__((jb->jb_name.ptr, 
			 "JB shrinking %d frame(s), cur size=%d", diff,
			 jb_framelist_eff_size(&jb->jb_framelist)));
	    }
	}
    }

#endif /* !PROGRESSIVE_DISCARD */

}

PJ_DEF(void) pjmedia_jbuf_put_frame( pjmedia_jbuf *jb, 
				     const void *frame, 
				     pj_size_t frame_size, 
				     int frame_seq)
{
    pjmedia_jbuf_put_frame2(jb, frame, frame_size, 0, frame_seq, NULL);
}

PJ_DEF(void) pjmedia_jbuf_put_frame2(pjmedia_jbuf *jb, 
				     const void *frame, 
				     pj_size_t frame_size, 
				     pj_uint32_t bit_info,
				     int frame_seq,
				     pj_bool_t *discarded)
{
    pj_size_t min_frame_size;
    int new_size, cur_size, frame_type = PJMEDIA_JB_NORMAL_FRAME;
    pj_status_t status;

    cur_size = jb_framelist_eff_size(&jb->jb_framelist);

#if PROGRESSIVE_DISCARD
    {
	unsigned interval, seq_delta;
	unsigned burst_level, burst_factor;

	/* Calculating discard interval (aggressiveness) based on
	 * (current size / burst level).
	 */
	if (jb->jb_status == JB_STATUS_PROCESSING) {
	    burst_level = PJ_MAX(jb->jb_eff_level, jb->jb_level);
	    burst_factor = cur_size / burst_level;
	    /* Tolerate small spikes */
	    if ((burst_level <= 5) && (burst_factor < 3))
		burst_factor = 0;
	} else {
	    burst_factor = 0;
	}

	switch (burst_factor) {
	case 0:
	    interval = 0;
	    break;
	case 1:
	    interval = 7;
	    break;
	case 2:
	    interval = 5;
	    break;
	default:
	    interval = 4;
	    break;
	}

	/* Do the math now to see if we should discard this packet.
	 * Calculate the distance from the last sequence
	 * discarded. If negative, then this is an out of
	 * order frame so just proceed with discard. Else
	 * see if the delta is at least the intervals worth away
	 * from the last frame discarded.
	 */
	seq_delta = (pj_uint16_t)(frame_seq - jb->jb_last_discard_seq);
	if ((0 != interval) && (seq_delta >= interval)) {
	    frame_type = PJMEDIA_JB_DISCARDED_FRAME;
	    jb->jb_last_discard_seq = frame_seq;

	    TRACE__((jb->jb_name.ptr, 
		    "Discarding frame #%d: eff=%d disc=%d orig:%d"
		    " seq_delta:%d",
		    frame_seq,
		    cur_size,
		    jb_framelist_size(&jb->jb_framelist) - cur_size,
		    jb_framelist_origin(&jb->jb_framelist),
		    (int)seq_delta));
	}
    }
#endif /* PROGRESSIVE_DISCARD */
    

    /* Attempt to store the frame */
    min_frame_size = PJ_MIN(frame_size, jb->jb_frame_size);
    status = jb_framelist_put_at(&jb->jb_framelist, frame_seq, frame,
				 min_frame_size, bit_info, frame_type);
    
    /* Jitter buffer is full, remove some older frames */
    while (status == PJ_ETOOMANY) {
	int distance;
	unsigned removed;

	/* When progressive discard activated, just remove as few as possible
	 * just to make this frame in.
	 */
#if PROGRESSIVE_DISCARD
	/* The cases of seq-jump, out-of-order, and seq restart should have
	 * been handled/normalized by previous call of jb_framelist_put_at().
	 * So we're confident about 'distance' value here.
	 */
	distance = (frame_seq - jb_framelist_origin(&jb->jb_framelist)) -
		   jb->jb_max_count + 1;
	pj_assert(distance > 0);
#else
	distance = PJ_MAX(jb->jb_max_count/4, 1);
#endif
	removed = jb_framelist_remove_head(&jb->jb_framelist, distance);
	status = jb_framelist_put_at(&jb->jb_framelist, frame_seq, frame,
				     min_frame_size, bit_info, frame_type);

	jb->jb_discard += removed;
    }

    /* Get new JB size after PUT */
    new_size = jb_framelist_eff_size(&jb->jb_framelist);

    /* Return the flag if this frame is discarded */
    if (discarded)
	*discarded = (status != PJ_SUCCESS);

    if (status == PJ_SUCCESS) {
	if (jb->jb_status == JB_STATUS_PREFETCHING) {
	    TRACE__((jb->jb_name.ptr, "PUT prefetch_cnt=%d/%d", 
		     new_size, jb->jb_prefetch));
	    if (new_size >= jb->jb_prefetch)
		jb->jb_status = JB_STATUS_PROCESSING;
	}
	jb->jb_level += (new_size > cur_size ? new_size-cur_size : 1);
	jbuf_update(jb, JB_OP_PUT);
    } else
	jb->jb_discard++;
}

/*
 * Get frame from jitter buffer.
 */
PJ_DEF(void) pjmedia_jbuf_get_frame( pjmedia_jbuf *jb, 
				     void *frame, 
				     char *p_frame_type)
{
    pjmedia_jbuf_get_frame2(jb, frame, NULL, p_frame_type, NULL);
}

/*
 * Get frame from jitter buffer.
 */
PJ_DEF(void) pjmedia_jbuf_get_frame2(pjmedia_jbuf *jb, 
				     void *frame, 
				     pj_size_t *size,
				     char *p_frame_type,
				     pj_uint32_t *bit_info)
{
    if (jb->jb_status == JB_STATUS_PREFETCHING) {

	/* Can't return frame because jitter buffer is filling up
	 * minimum prefetch.
	 */

	//pj_bzero(frame, jb->jb_frame_size);
	*p_frame_type = PJMEDIA_JB_ZERO_PREFETCH_FRAME;
	if (size)
	    *size = 0;

	TRACE__((jb->jb_name.ptr, "GET prefetch_cnt=%d/%d",
		 jb_framelist_eff_size(&jb->jb_framelist), jb->jb_prefetch));

	jb->jb_empty++;

    } else {

	pjmedia_jb_frame_type ftype = PJMEDIA_JB_NORMAL_FRAME;
	pj_bool_t res;

	/* Try to retrieve a frame from frame list */
	res = jb_framelist_get(&jb->jb_framelist, frame, size, &ftype, 
			       bit_info);
	if (res) {
	    /* We've successfully retrieved a frame from the frame list, but
	     * the frame could be a blank frame!
	     */
	    if (ftype == PJMEDIA_JB_NORMAL_FRAME) {
		*p_frame_type = PJMEDIA_JB_NORMAL_FRAME;
	    } else {
		*p_frame_type = PJMEDIA_JB_MISSING_FRAME;
		jb->jb_lost++;
	    }

	    /* Store delay history at the first GET */
	    if (jb->jb_last_op == JB_OP_PUT) {
		unsigned cur_size;

		/* We've just retrieved one frame, so add one to cur_size */
		cur_size = jb_framelist_eff_size(&jb->jb_framelist) + 1;
		pj_math_stat_update(&jb->jb_delay, 
				    cur_size*jb->jb_frame_ptime);
	    }
	} else {
	    /* Jitter buffer is empty */
	    if (jb->jb_prefetch)
		jb->jb_status = JB_STATUS_PREFETCHING;

	    //pj_bzero(frame, jb->jb_frame_size);
	    *p_frame_type = PJMEDIA_JB_ZERO_EMPTY_FRAME;
	    if (size)
		*size = 0;

	    jb->jb_empty++;
	}
    }

    jb->jb_level++;
    jbuf_update(jb, JB_OP_GET);
}

/*
 * Get jitter buffer state.
 */
PJ_DEF(pj_status_t) pjmedia_jbuf_get_state( const pjmedia_jbuf *jb,
					    pjmedia_jb_state *state )
{
    PJ_ASSERT_RETURN(jb && state, PJ_EINVAL);

    state->frame_size = jb->jb_frame_size;
    state->min_prefetch = jb->jb_min_prefetch;
    state->max_prefetch = jb->jb_max_prefetch;
    
    state->burst = jb->jb_eff_level;
    state->prefetch = jb->jb_prefetch;
    state->size = jb_framelist_eff_size(&jb->jb_framelist);
    
    state->avg_delay = jb->jb_delay.mean;
    state->min_delay = jb->jb_delay.min;
    state->max_delay = jb->jb_delay.max;
    state->dev_delay = pj_math_stat_get_stddev(&jb->jb_delay);
    
    state->avg_burst = jb->jb_burst.mean;
    state->empty = jb->jb_empty;
    state->discard = jb->jb_discard;
    state->lost = jb->jb_lost;

    return PJ_SUCCESS;
}

