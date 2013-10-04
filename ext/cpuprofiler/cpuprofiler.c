/**********************************************************************

  cpuprofiler.c - CPU profiler for MRI.

  $Author$
  created at: Thu May 30 17:55:25 2013

  NOTE: This extension library is not expected to exist except C Ruby.

  All the files in this distribution are covered under the Ruby's
  license (see the file COPYING).

**********************************************************************/

#include <ruby/ruby.h>
#include <ruby/debug.h>
#include <ruby/st.h>
#include <sys/time.h>
#include "internal.h"

typedef struct {
    size_t total_samples;
    size_t caller_samples;
    st_table *edges;
} iseq_data_t;

static struct {
    size_t overall_samples;
    st_table *iseqs;
} _results;
static VALUE profiler_proc;

static VALUE
cpuprofiler_start(VALUE self, VALUE usec)
{
    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = NUM2LONG(usec);
    timer.it_value = timer.it_interval;
    setitimer(ITIMER_PROF, &timer, 0);

    return Qnil;
}

static VALUE
cpuprofiler_stop(VALUE self)
{
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    setitimer(ITIMER_PROF, &timer, 0);

    return Qnil;
}

static int
sample_edges_i(st_data_t key, st_data_t val, st_data_t arg)
{
    VALUE edges = (VALUE)arg;
    VALUE iseq = (VALUE)key;
    intptr_t weight = (intptr_t)val;
    rb_hash_aset(edges, iseq, INT2FIX(weight));
    return ST_CONTINUE;
}

static int
sample_i(st_data_t key, st_data_t val, st_data_t arg)
{
    iseq_data_t *iseq_data = (iseq_data_t *)val;
    VALUE results = (VALUE)arg;
    VALUE details = rb_hash_new();
    VALUE edges = Qnil;

    rb_hash_aset(results, (VALUE)key, details);
    rb_hash_aset(details, ID2SYM(rb_intern("total_samples")), SIZET2NUM(iseq_data->total_samples));
    rb_hash_aset(details, ID2SYM(rb_intern("caller_samples")), SIZET2NUM(iseq_data->caller_samples));
    if (iseq_data->edges) {
        edges = rb_hash_new();
        rb_hash_aset(details, ID2SYM(rb_intern("edges")), edges);
        st_foreach(iseq_data->edges, sample_edges_i, (st_data_t)edges);
        st_free_table(iseq_data->edges);
        iseq_data->edges = NULL;
    }

    xfree(iseq_data);
    return ST_DELETE;
}

static VALUE
cpuprofiler_run(VALUE self, VALUE usec)
{
    VALUE results;
    rb_need_block();
    _results.overall_samples = 0;

    cpuprofiler_start(self, usec);
    rb_yield(Qundef);
    cpuprofiler_stop(self);

    results = rb_hash_new();
    rb_hash_aset(results, ID2SYM(rb_intern("overall_samples")), SIZET2NUM(_results.overall_samples));
    st_foreach(_results.iseqs, sample_i, (st_data_t)results);
    return results;
}

static inline iseq_data_t *
sample_for(VALUE iseq)
{
    st_data_t key = (st_data_t)iseq, val = 0;
    iseq_data_t *iseq_data;

    if (st_lookup(_results.iseqs, key, &val)) {
        iseq_data = (iseq_data_t *)val;
    } else {
        iseq_data = ALLOC_N(iseq_data_t, 1);
        MEMZERO(iseq_data, iseq_data_t, 1);
        val = (st_data_t)iseq_data;
        st_insert(_results.iseqs, key, val);
    }

    return iseq_data;
}

static VALUE
each_iseq(VALUE iseq, int line, void *ctx)
{
    static VALUE prev;
    st_data_t key;
    intptr_t *index = ctx, weight = 0;
    iseq_data_t *iseq_data;

    iseq_data = sample_for(iseq);
    iseq_data->total_samples++;

    if ((*index)++ == 0) {
        iseq_data->caller_samples++;
    } else {
        if (!iseq_data->edges)
            iseq_data->edges = st_init_numtable();

        key = prev;
        st_lookup(iseq_data->edges, key, (st_data_t *)&weight);
        weight++;
        st_insert(iseq_data->edges, key, weight);
    }

    prev = iseq;
    return Qtrue;
}

static void
cpuprofiler_sample()
{
    intptr_t index = 0;
    _results.overall_samples++;
    rb_callstack_iseq_each(each_iseq, &index);
}

static VALUE
cpuprofiler_signal_handler(VALUE arg, VALUE ctx)
{
    static int in_signal_handler = 0;
    if (in_signal_handler) return Qnil;

    in_signal_handler++;
    cpuprofiler_sample();
    in_signal_handler--;

    return Qnil;
}

void
Init_cpuprofiler(void)
{
    VALUE rb_mCpuProfiler = rb_define_module("CpuProfiler");
    rb_define_singleton_method(rb_mCpuProfiler, "run", cpuprofiler_run, 1);

    /* signal handlers in C could fire during method dispatch, gc, etc.
     * we use a ruby signal handler to ensure consistent VM state
     * during callstack samples.
     *
     * TODO: use postponed_job api from real signal handler?
     */
    profiler_proc = rb_proc_new(cpuprofiler_signal_handler, Qnil);
    rb_global_variable(&profiler_proc);
    rb_funcall(Qnil, rb_intern("trap"), 2, rb_str_new_cstr("PROF"), profiler_proc);

    /* TODO: remove global
     */
    _results.iseqs = st_init_numtable();
}
