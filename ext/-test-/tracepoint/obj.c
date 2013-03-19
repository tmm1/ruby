#include "ruby.h"
#include "ruby/debug.h"

static size_t count;
static VALUE objects[10];

void
newobj_callback(VALUE tpval, void *data)
{
    rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
    VALUE obj = rb_tracearg_object(tparg);

    if (count < sizeof(objects))
        objects[count++] = obj;
}

VALUE
tracepoint_track_newobj(VALUE self)
{
    VALUE tpval = rb_tracepoint_new(0, RUBY_EVENT_OBJ_NEW, newobj_callback, 0);

    count = 0;
    memset(objects, (VALUE)Qnil, sizeof(objects));

    rb_tracepoint_enable(tpval);
    rb_yield(Qundef);
    rb_tracepoint_disable(tpval);

    return rb_ary_new4(count, objects);
}

void
Init_tracepoint(void)
{
    VALUE mBug = rb_define_module("Bug");
    rb_define_module_function(mBug, "tracepoint_track_newobj", tracepoint_track_newobj, 0);
}
