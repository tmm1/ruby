#include "ruby.h"
#include "ruby/debug.h"

static void
task_callback(void *data)
{
    VALUE ary = (VALUE)data;
    Check_Type(ary, T_ARRAY);

    rb_ary_replace(ary, rb_funcall(Qnil, rb_intern("caller"), 0));
}

static VALUE
task_enqueue_caller(VALUE self, VALUE obj)
{
    rb_task_enqueue(task_callback, (void *)obj);
}

static VALUE
task_direct_caller(VALUE self, VALUE obj)
{
    task_callback((void *)obj);
}

void
Init_task(VALUE self)
{
    VALUE mBug = rb_define_module("Bug");
    rb_define_module_function(mBug, "task_enqueue_caller", task_enqueue_caller, 1);
    rb_define_module_function(mBug, "task_direct_caller", task_direct_caller, 1);
}
