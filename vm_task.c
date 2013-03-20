/**********************************************************************

  vm_task.c -

  Copyright (C) 1993-2013 Yukihiro Matsumoto

**********************************************************************/

#include "ruby/ruby.h"
#include "internal.h"
#include "vm_core.h"

typedef void (*rb_task_func_t)(void *arg);

typedef struct rb_task_job_struct {
    rb_task_func_t func;
    void *data;
    struct rb_task_job_struct *next;
} rb_task_job_t;

static rb_task_job_t *job_list;

void
rb_task_run_deferred(void)
{
    rb_task_job_t *job;

    rb_gc_finalize_deferred();

    while (job_list) {
        job = job_list;
        job_list = job->next;

        job->func(job->data);
        xfree(job);
    }
}

void
rb_task_enqueue(rb_task_func_t func, void *data)
{
    rb_thread_t *th = GET_THREAD();

    rb_task_job_t *job = ALLOC(rb_task_job_t);
    job->func = func;
    job->data = data;
    job->next = job_list;
    job_list = job;

    if (th) {
        RUBY_VM_SET_TASK_INTERRUPT(th);
    }
}
