/**********************************************************************

  frameprofiler.c - Call-stack frame profiler for MRI.

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

#define BUF_SIZE 2048

typedef struct {
    size_t total_samples;
    size_t caller_samples;
    st_table *edges;
    st_table *lines;
} frame_data_t;

static struct {
    size_t overall_samples;
    st_table *frames;

    VALUE frames_buffer[BUF_SIZE];
    int lines_buffer[BUF_SIZE];
} _results;
static VALUE profiler_proc;

static VALUE
frameprofiler_start(VALUE self, VALUE usec)
{
    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = NUM2LONG(usec);
    timer.it_value = timer.it_interval;
    setitimer(ITIMER_PROF, &timer, 0);

    return Qnil;
}

static VALUE
frameprofiler_stop(VALUE self)
{
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    setitimer(ITIMER_PROF, &timer, 0);

    return Qnil;
}

static int
frame_edges_i(st_data_t key, st_data_t val, st_data_t arg)
{
    VALUE edges = (VALUE)arg;

    intptr_t weight = (intptr_t)val;
    rb_hash_aset(edges, rb_obj_id((VALUE)key), INT2FIX(weight));
    return ST_CONTINUE;
}

static int
frame_lines_i(st_data_t key, st_data_t val, st_data_t arg)
{
    VALUE lines = (VALUE)arg;

    intptr_t weight = (intptr_t)val;
    rb_hash_aset(lines, INT2FIX(key), INT2FIX(weight));
    return ST_CONTINUE;
}

static int
frame_i(st_data_t key, st_data_t val, st_data_t arg)
{
    VALUE frame = (VALUE)key;
    frame_data_t *frame_data = (frame_data_t *)val;
    VALUE results = (VALUE)arg;
    VALUE details = rb_hash_new();
    VALUE name, location, edges, lines;
    VALUE label, method_name;
    int line;

    rb_hash_aset(results, rb_obj_id(frame), details);

    name = rb_profile_frame_full_label(frame);
    location = rb_profile_frame_absolute_path(frame);
    if (NIL_P(location))
	location = rb_profile_frame_path(frame);
    if ((line = rb_profile_frame_first_lineno(frame)) != INT2FIX(0))
	location = rb_sprintf("%"PRIsVALUE":%u", location, line);

    rb_hash_aset(details, ID2SYM(rb_intern("name")), name);
    rb_hash_aset(details, ID2SYM(rb_intern("location")), location);

    rb_hash_aset(details, ID2SYM(rb_intern("total_samples")), SIZET2NUM(frame_data->total_samples));
    rb_hash_aset(details, ID2SYM(rb_intern("samples")), SIZET2NUM(frame_data->caller_samples));

    if (frame_data->edges) {
        edges = rb_hash_new();
        rb_hash_aset(details, ID2SYM(rb_intern("edges")), edges);
        st_foreach(frame_data->edges, frame_edges_i, (st_data_t)edges);
        st_free_table(frame_data->edges);
        frame_data->edges = NULL;
    }

    if (frame_data->lines) {
	lines = rb_hash_new();
	rb_hash_aset(details, ID2SYM(rb_intern("lines")), lines);
	st_foreach(frame_data->lines, frame_lines_i, (st_data_t)lines);
	st_free_table(frame_data->lines);
	frame_data->lines = NULL;
    }

    xfree(frame_data);
    return ST_DELETE;
}

static VALUE
frameprofiler_run(VALUE self, VALUE usec)
{
    VALUE results, frames;
    rb_need_block();
    _results.overall_samples = 0;

    frameprofiler_start(self, usec);
    rb_yield(Qundef);
    frameprofiler_stop(self);

    results = rb_hash_new();
    rb_hash_aset(results, ID2SYM(rb_intern("version")), DBL2NUM(1.0));
    rb_hash_aset(results, ID2SYM(rb_intern("mode")), rb_sprintf("cpu(%"PRIsVALUE")", usec));
    rb_hash_aset(results, ID2SYM(rb_intern("samples")), SIZET2NUM(_results.overall_samples));

    frames = rb_hash_new();
    rb_hash_aset(results, ID2SYM(rb_intern("frames")), frames);
    st_foreach(_results.frames, frame_i, (st_data_t)frames);

    return results;
}

static inline frame_data_t *
sample_for(VALUE frame)
{
    st_data_t key = (st_data_t)frame, val = 0;
    frame_data_t *frame_data;

    if (st_lookup(_results.frames, key, &val)) {
        frame_data = (frame_data_t *)val;
    } else {
        frame_data = ALLOC_N(frame_data_t, 1);
        MEMZERO(frame_data, frame_data_t, 1);
        val = (st_data_t)frame_data;
        st_insert(_results.frames, key, val);
    }

    return frame_data;
}

void
st_numtable_increment(st_table *table, st_data_t key)
{
    intptr_t weight = 0;
    st_lookup(table, key, (st_data_t *)&weight);
    weight++;
    st_insert(table, key, weight);
}

static void
frameprofiler_sample()
{
    int num, i;
    VALUE prev_frame;
    st_data_t key;

    _results.overall_samples++;
    num = rb_profile_frames(0, sizeof(_results.frames_buffer), _results.frames_buffer, _results.lines_buffer);

    for (i = 0; i < num; i++) {
	int line = _results.lines_buffer[i];
	VALUE frame = _results.frames_buffer[i];
	frame_data_t *frame_data = sample_for(frame);

	frame_data->total_samples++;

	if (i == 0) {
	    frame_data->caller_samples++;
	    if (line > 0) {
		if (!frame_data->lines)
		    frame_data->lines = st_init_numtable();
		st_numtable_increment(frame_data->lines, (st_data_t)line);
	    }
	} else {
	    if (!frame_data->edges)
		frame_data->edges = st_init_numtable();
	    st_numtable_increment(frame_data->edges, (st_data_t)prev_frame);
	}

	prev_frame = frame;
    }
}

static VALUE
frameprofiler_signal_handler(VALUE arg, VALUE ctx)
{
    static int in_signal_handler = 0;
    if (in_signal_handler) return Qnil;

    in_signal_handler++;
    frameprofiler_sample();
    in_signal_handler--;

    return Qnil;
}

void
Init_frameprofiler(void)
{
    VALUE rb_mFrameProfiler = rb_define_module("FrameProfiler");
    rb_define_singleton_method(rb_mFrameProfiler, "run", frameprofiler_run, 1);

    /* signal handlers in C could fire during method dispatch, gc, etc.
     * we use a ruby signal handler to ensure consistent VM state
     * during callstack samples.
     *
     * TODO: use postponed_job api from real signal handler?
     */
    profiler_proc = rb_proc_new(frameprofiler_signal_handler, Qnil);
    rb_global_variable(&profiler_proc);
    rb_funcall(Qnil, rb_intern("trap"), 2, rb_str_new_cstr("PROF"), profiler_proc);

    /* TODO: remove global
     */
    _results.frames = st_init_numtable();
}
