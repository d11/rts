/* Stackless module implementation */

#include "Python.h"

#ifdef STACKLESS
#include "core/stackless_impl.h"

#define IMPLEMENT_STACKLESSMODULE
#include "platf/slp_platformselect.h"
#include "core/cframeobject.h"
#include "taskletobject.h"
#include "channelobject.h"
#include "pickling/prickelpit.h"
#include "core/stackless_methods.h"

/******************************************************

  The Stackless Module

 ******************************************************/

PyObject *slp_module = NULL;

PyThreadState *slp_initial_tstate = NULL;

static char schedule__doc__[] =
"schedule(retval=stackless.current) -- switch to the next runnable tasklet.\n\
The return value for this call is retval, with the current\n\
tasklet as default.\n\
schedule_remove(retval=stackless.current) -- ditto, and remove self.";

static PyObject *
PyStackless_Schedule_M(PyObject *retval, int remove)
{
	return PyStackless_CallMethod_Main(
	    slp_module,
	    remove ? "schedule_remove" : "schedule",
	    "O", retval);
}

PyObject *
PyStackless_Schedule(PyObject *retval, int remove)
{
    STACKLESS_GETARG();
    PyThreadState *ts = PyThreadState_GET();
    PyTaskletObject *prev = ts->st.current, *next = prev->next;
    PyObject *ret = NULL;
    int switched;

    if (ts->st.main == NULL) return PyStackless_Schedule_M(retval, remove);
    /* make sure we hold a reference to the previous tasklet */
    Py_INCREF(prev);
    TASKLET_SETVAL(prev, retval);
    if (remove) {
        slp_current_remove();
        Py_DECREF(prev);
        if (next == prev)
            next = 0; /* we were the last runnable tasklet */
    }
    /* we mustn't DECREF prev here (after the slp_schedule_task().
     * This could be the last reference, thus
     * promting emergency reactivation of the tasklet,
     * and soft switching isn't really done until we have unwound.
     * Use the delayed release mechanism instead.
     */
    assert(ts->st.del_post_switch == NULL);
    ts->st.del_post_switch = (PyObject*)prev;

    ret = slp_schedule_task(prev, next, stackless, &switched);

    /* however, if this was a no-op (e.g. prev==next, or an error occurred)
     * we need to decref prev ourselves
     */
    if (!switched)
        Py_CLEAR(ts->st.del_post_switch);
    return ret;
}

PyObject *
PyStackless_Schedule_nr(PyObject *retval, int remove)
{
	STACKLESS_PROPOSE_ALL();
	return PyStackless_Schedule(retval, remove);
}

static PyObject *
schedule_generic(PyObject *self, PyObject *args, PyObject *kwds, int remove)
{
	STACKLESS_GETARG();
	PyObject *retval = (PyObject *) PyThreadState_GET()->st.current;
	static char *argnames[] = {"retval", NULL};

	if (PyTuple_GET_SIZE(args) > 0) {
		if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:schedule",
						 argnames, &retval))
			return NULL;
	}
	STACKLESS_PROMOTE_ALL();
	return PyStackless_Schedule(retval, remove);
}

static PyObject *
schedule(PyObject *self, PyObject *args, PyObject *kwds)
{
	return schedule_generic(self, args, kwds, 0);
}

static PyObject *
schedule_remove(PyObject *self, PyObject *args, PyObject *kwds)
{
	return schedule_generic(self, args, kwds, 1);
}


static char getruncount__doc__[] =
"getruncount() -- return the number of runnable tasklets.";

int
PyStackless_GetRunCount(void)
{
	PyThreadState *ts = PyThreadState_GET();
	return ts->st.runcount;
}

static PyObject *
getruncount(PyObject *self)
{
	PyThreadState *ts = PyThreadState_GET();
	return PyLong_FromLong(ts->st.runcount);
}


static char getcurrent__doc__[] =
"getcurrent() -- return the currently executing tasklet.";

PyObject *
PyStackless_GetCurrent(void)
{
	PyThreadState *ts = PyThreadState_GET();
	PyObject *t = (PyObject*)ts->st.current;

	Py_INCREF(t);
	return t;
}

static PyObject *
getcurrent(PyObject *self)
{
	return PyStackless_GetCurrent();
}

static char getmain__doc__[] =
"getmain() -- return the main tasklet of this thread.";

static PyObject *
getmain(PyObject *self)
{
	PyThreadState *ts = PyThreadState_GET();
	PyObject * t = (PyObject*) ts->st.main;
	Py_INCREF(t);
	return t;
}

static char enable_soft__doc__[] =
"enable_softswitch(flag) -- control the switching behavior.\n"
"Tasklets can be either switched by moving C stack slices around\n"
"or by avoiding stack changes at all. The latter is only possible\n"
"in the top interpreter level. Switching it off is for timing and\n"
"debugging purposes. This flag exists once for the whole process.\n"
"For inquiry only, use the phrase\n"
"   ret = enable_softswitch(0); enable_softswitch(ret)\n"
"By default, soft switching is enabled.";

static PyObject *
enable_softswitch(PyObject *self, PyObject *flag)
{
	PyObject *ret;
	if (! (flag && PyLong_Check(flag)) ) {
		PyErr_SetString(PyExc_TypeError,
		    "enable_softswitch needs exactly one bool or integer");
		return NULL;
	}
	ret = PyBool_FromLong(slp_enable_softswitch);
	slp_enable_softswitch = PyLong_AsLong(flag);
	return ret;
}


static char run_watchdog__doc__[] =
"run_watchdog(timeout=0, threadblock=False, soft=False,\n\
              ignore_nesting=False, totaltimeout=False) -- \n\
run tasklets until they are all\n\
done, or timeout instructions have passed, if timeout is not 0.\n\
Tasklets must provide cooperative schedule() calls.\n\
If the timeout is met, the function returns.\n\
This function must be called from the main tasklet only.\n\
If a tasklet is interrupted, it is returned as the return value of the\n\
function.\n\
If an exception occours, it will be passed to the main tasklet.\n\
The other optional flags paremeter work as follows:\n\
threadblock:  When set, ensures, that the thread\n\
will block when it runs out of tasklets to await channel activity from\n\
other Python threads to wake it up.\n\
soft:  When set, tasklets won't be interrupted but\n\
run will return (with a None) at the next convenient scheduling moment\n\
when the timeout has occurred.\n\
ignore_nesting: Allow interrupts at any interpreter nesting level,\n\
ignoring the tasklets' own ignore_nesting attribute.\n\
totaltimeout: The 'timeout' argument is the total timeout for run(),\n\
rather than a maximum timeslice for a single tasklet.  This for run()\n\
to return after a certain time.";

static PyObject *
interrupt_timeout_return(void)
{
	PyThreadState *ts = PyThreadState_GET();
	PyTaskletObject *current = ts->st.current;

	/*
	 * we mark the IRQ as pending if
	 * a) we are in soft interrupt mode
	 * b) we are in hard interrupt mode, but aren't allowed to break here which happens
	 * when "atomic" is set, or the "nesting level" is relevant and not ignored.
	 */
	if ((ts->st.runflags & PY_WATCHDOG_SOFT) ||
		current->flags.atomic || ts->st.schedlock ||
		!TASKLET_NESTING_OK(current))
	{
		ts->st.tick_watermark = ts->st.tick_counter + ts->st.interval;
		current->flags.pending_irq = 1;
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
		current->flags.pending_irq = 0;

    return slp_schedule_task(ts->st.current, ts->st.main, 1, 0);
}

static PyObject *
PyStackless_RunWatchdog_M(long timeout, long flags)
{
	return PyStackless_CallMethod_Main(slp_module, "run", "(ll)", timeout, flags);
}


PyObject *
PyStackless_RunWatchdog(long timeout)
{
	return PyStackless_RunWatchdogEx(timeout, 0);
}


PyObject *
PyStackless_RunWatchdogEx(long timeout, int flags)
{
	PyThreadState *ts = PyThreadState_GET();
	PyTaskletObject *victim;
	PyObject *retval;

	if (ts->st.main == NULL)
		return PyStackless_RunWatchdog_M(timeout, flags);
	if (ts->st.current != ts->st.main)
		RUNTIME_ERROR(
		    "run() must be run from the main tasklet.",
		    NULL);

	if (timeout <= 0)
		ts->st.interrupt = NULL;
	else
		ts->st.interrupt = interrupt_timeout_return;

	ts->st.interval = timeout;
	ts->st.tick_watermark = ts->st.tick_counter + timeout;

	/* remove main. Will get back at the end. */
	slp_current_remove();
	Py_DECREF(ts->st.main);

    /* now let them run until the end. */
    ts->st.runflags = flags;
    retval = slp_schedule_task(ts->st.main, ts->st.current, 0, 0);
    ts->st.runflags = 0;
    ts->st.interrupt = NULL;

	/* retval really should be PyNone here (or NULL).  Technically, it is the
	 * tempval of some tasklet that has quit.  Even so, it is quite
	 * useless to use.  run() must return None, or a tasklet
	 */
	Py_XDECREF(retval);
	if (retval == NULL) /* an exception has occoured */
		return NULL;

	/*
	 * back in main.
	 * We were either revived by slp_tasklet_end or the interrupt.
	 * If we were using hard interrupts (bit 1 in flags not set)
	 * we need to return the interrupted tasklet)
	 */
	if (ts->st.runcount > 1 && !(flags & PY_WATCHDOG_SOFT)) {
		/* remove victim. It is sitting next to us. */
		ts->st.current = (PyTaskletObject*)ts->st.main->next;
		victim = slp_current_remove();
		ts->st.current = (PyTaskletObject*)ts->st.main;
		return (PyObject*) victim;
	} else
		Py_RETURN_NONE;
}

static PyObject *
run_watchdog(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *argnames[] = {"timeout", "threadblock", "soft",
								"ignore_nesting", "totaltimeout",
								NULL};
	long timeout = 0;
	int threadblock = 0;
	int soft = 0;
	int ignore_nesting = 0;
	int totaltimeout = 0;
	int flags;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|liiii:run_watchdog",
					 argnames, &timeout, &threadblock, &soft,
					 &ignore_nesting, &totaltimeout))
		return NULL;
	flags = threadblock ? Py_WATCHDOG_THREADBLOCK : 0;
	flags |= soft ? PY_WATCHDOG_SOFT : 0;
	flags |= ignore_nesting ? PY_WATCHDOG_IGNORE_NESTING : 0;
	flags |= totaltimeout ? PY_WATCHDOG_TOTALTIMEOUT : 0;
	return PyStackless_RunWatchdogEx(timeout, flags);
}

static char get_thread_info__doc__[] =
"get_thread_info(thread_id) -- return a 3-tuple of the thread's\n\
main tasklet, current tasklet and runcount.\n\
To obtain a list of all thread infos, use\n\
\n\
map (stackless.get_thread_info, stackless.threads)";

static PyObject *
get_thread_info(PyObject *self, PyObject *args)
{
	PyThreadState *ts = PyThreadState_GET();
	PyInterpreterState *interp = ts->interp;
	long id = 0;

	if (!PyArg_ParseTuple(args, "|l:get_thread_info", &id))
		return NULL;
	for (ts = interp->tstate_head; id && ts != NULL; ts = ts->next) {
		if (ts->thread_id == id)
			break;
	}
	if (ts == NULL)
		RUNTIME_ERROR("Thread id not found", NULL);

	return Py_BuildValue("(OOi)",
	    ts->st.main ? (PyObject *) ts->st.main : Py_None,
	    ts->st.runcount ? (PyObject *) ts->st.current : Py_None,
	    ts->st.runcount);
}

static PyObject *
slpmodule_reduce(PyObject *self)
{
	return PyObject_GetAttrString(slp_module, "__name__");
}

/******************************************************

  some test functions

 ******************************************************/


static char test_cframe__doc__[] =
"test_cframe(switches, words=0) -- a builtin testing function that does nothing\n\
but tasklet switching. The function will call PyStackless_Schedule() for switches\n\
times and then finish.\n\
If words is given, as many words will be allocated on the C stack.\n\
Usage: Create two tasklets for test_cframe and run them by run().\n\
\n\
    t1 = tasklet(test_cframe)(500000)\n\
    t2 = tasklet(test_cframe)(500000)\n\
    run()\n\
This can be used to measure the execution time of 1.000.000 switches.";

/* we define the stack max as the typical recursion limit
 * times the typical number of words per recursion.
 * That is 1000 * 64
 */

#define STACK_MAX_USEFUL 64000
#define STACK_MAX_USESTR "64000"

static
PyObject *
test_cframe(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *argnames[] = {"switches", "words", NULL};
	long switches, extra = 0;
	long i;
	PyObject *ret = Py_None;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "l|l:test_cframe",
					 argnames, &switches, &extra))
		return NULL;
		if (extra < 0 || extra > STACK_MAX_USEFUL)
			VALUE_ERROR(
				"test_cframe: words are limited by 0 and " \
				STACK_MAX_USESTR, NULL);
	if (extra > 0)
		alloca(extra*sizeof(PyObject*));
	Py_INCREF(ret);
	for (i=0; i<switches; i++) {
		Py_DECREF(ret);
		ret = PyStackless_Schedule(Py_None, 0);
		if (ret == NULL) return NULL;
	}
	return ret;
}

static char test_outside__doc__[] =
"test_outside() -- a builtin testing function.\n\
This function simulates an application that does not run \"inside\"\n\
Stackless, with active, running frames, but always needs to initialize\n\
the main tasklet to get \"inside\".\n\
The function will terminate when no other tasklets are runnable.\n\
\n\
Typical usage: Create a tasklet for test_cframe and run by test_outside().\n\
\n\
    t1 = tasklet(test_cframe)(1000000)\n\
    test_outside()\n\
This can be used to measure the execution time of 1.000.000 switches.";

static
PyObject *
test_outside(PyObject *self)
{
	PyThreadState *ts = PyThreadState_GET();
	PyTaskletObject *stmain = ts->st.main;
	PyCStackObject *cst = ts->st.initial_stub;
	PyFrameObject *f = ts->frame;
	int recursion_depth = ts->recursion_depth;
	int nesting_level = ts->st.nesting_level;
	PyObject *ret = Py_None;
	int jump = ts->st.serial_last_jump;

	Py_INCREF(ret);
	ts->st.main = NULL;
	ts->st.initial_stub = NULL;
	ts->st.nesting_level = 0;
	ts->frame = NULL;
	ts->recursion_depth = 0;
	slp_current_remove();
	while (ts->st.runcount > 0) {
		Py_DECREF(ret);
		ret = PyStackless_Schedule(Py_None, 0);
		if (ret == NULL) {
			break;
		}
	}
	ts->st.main = stmain;
	Py_CLEAR(ts->st.initial_stub);
	ts->st.initial_stub = cst;
	ts->frame = f;
	slp_current_insert(stmain);
	ts->recursion_depth = recursion_depth;
	ts->st.nesting_level = nesting_level;
	ts->st.serial_last_jump = jump;
	return ret;
}


static char test_cframe_nr__doc__[] =
"test_cframe_nr(switches) -- a builtin testing function that does nothing\n\
but soft tasklet switching. The function will call PyStackless_Schedule_nr() for switches\n\
times and then finish.\n\
Usage: Cf. test_cframe().";

static
PyObject *
test_cframe_nr_loop(PyFrameObject *f, int exc, PyObject *retval)
{
	PyThreadState *ts = PyThreadState_GET();
	PyCFrameObject *cf = (PyCFrameObject *) f;

	if (retval == NULL)
		goto exit_test_cframe_nr_loop;
	while (cf->n-- > 0) {
		Py_DECREF(retval);
		retval = PyStackless_Schedule_nr(Py_None, 0);
		if (retval == NULL) {
			ts->frame = cf->f_back;
			return NULL;
		}
		if (STACKLESS_UNWINDING(retval)) {
			return retval;
		}
		/* was a hard switch */
	}
exit_test_cframe_nr_loop:
	ts->frame = cf->f_back;
	Py_DECREF(cf);
	return retval;
}

static
PyObject *
test_cframe_nr(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *argnames[] = {"switches", NULL};
	PyThreadState *ts = PyThreadState_GET();
	PyCFrameObject *cf;
	long switches;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "l:test_cframe_nr",
					 argnames, &switches))
		return NULL;
	cf = slp_cframe_new(test_cframe_nr_loop, 1);
	if (cf == NULL)
		return NULL;
	cf->n = switches;
	ts->frame = (PyFrameObject *) cf;
	Py_INCREF(Py_None);
	return STACKLESS_PACK(Py_None);
}


static char test_cstate__doc__[] =
"test_cstate(callable) -- a builtin testing function that simply returns the\n\
result of calling its single argument.  Used for testing to force stackless\n\
into using hard switching.";

static
PyObject *
test_cstate(PyObject *f, PyObject *callable)
{
	return PyObject_CallObject(callable, NULL);
}


/******************************************************

  The Stackless External Interface

 ******************************************************/

PyObject *
PyStackless_Call_Main(PyObject *func, PyObject *args, PyObject *kwds)
{
	PyThreadState *ts = PyThreadState_GET();
	PyCFrameObject *c;
	PyObject *retval;

	if (ts->st.main != NULL)
		RUNTIME_ERROR(
		    "Call_Main cannot run within a main tasklet", NULL);
	c = slp_cframe_newfunc(func, args, kwds, 0);
	if (c == NULL)
		return NULL;
	/* frames eat their own reference when returning */
	Py_INCREF((PyObject *)c);
	retval = slp_eval_frame((PyFrameObject *) c);
	Py_DECREF((PyObject *)c);
	return retval;
}

/* this one is shamelessly copied from PyObject_CallMethod */

PyObject *
PyStackless_CallMethod_Main(PyObject *o, char *name, char *format, ...)
{
	va_list va;
	PyObject *args, *func = NULL, *retval;

	if (o == NULL || name == NULL)
		return slp_null_error();

	func = PyObject_GetAttrString(o, name);
	if (func == NULL) {
		PyErr_SetString(PyExc_AttributeError, name);
		return NULL;
	}

	if (!PyCallable_Check(func))
		return slp_type_error("call of non-callable attribute");

	if (format && *format) {
		va_start(va, format);
		args = Py_VaBuildValue(format, va);
		va_end(va);
	}
	else
		args = PyTuple_New(0);

	if (!args)
		return NULL;

	if (!PyTuple_Check(args)) {
		PyObject *a;

		a = PyTuple_New(1);
		if (a == NULL)
			return NULL;
		if (PyTuple_SetItem(a, 0, args) < 0)
			return NULL;
		args = a;
	}

	retval = PyStackless_Call_Main(func, args, NULL);

	Py_DECREF(args);
	Py_DECREF(func);

	return retval;
}

void PyStackless_SetScheduleFastcallback(slp_schedule_hook_func func)
{
	_slp_schedule_fasthook = func;
}

int PyStackless_SetScheduleCallback(PyObject *callable)
{
	if(callable != NULL && !PyCallable_Check(callable))
		TYPE_ERROR("schedule callback must be callable", -1);
	Py_XINCREF(callable);
	Py_CLEAR(_slp_schedule_hook);
	_slp_schedule_hook = callable;
	if (callable!=NULL)
		PyStackless_SetScheduleFastcallback(slp_schedule_callback);
	else
		PyStackless_SetScheduleFastcallback(NULL);
	return 0;
}

static char set_schedule_callback__doc__[] =
"set_schedule_callback(callable) -- install a callback for scheduling.\n\
Every explicit or implicit schedule will call the callback function.\n\
Example:\n\
  def schedule_cb(prev, next):\n\
      ...\n\
When a tasklet is dying, next is None.\n\
When main starts up or after death, prev is None.\n\
Pass None to switch monitoring off again.";

static PyObject *
set_schedule_callback(PyObject *self, PyObject *arg)
{
	if (arg == Py_None)
		arg = NULL;
	if (PyStackless_SetScheduleCallback(arg))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}


static char set_channel_callback__doc__[] =
"set_channel_callback(callable) -- install a callback for channels.\n\
Every send/receive action will call the callback function.\n\
Example:\n\
  def channel_cb(channel, tasklet, sending, willblock):\n\
      ...\n\
sending and willblock are integers.\n\
Pass None to switch monitoring off again.";

static PyObject *
set_channel_callback(PyObject *self, PyObject *arg)
{
	if (arg == Py_None)
		arg = NULL;
	if (PyStackless_SetChannelCallback(arg))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}


/******************************************************

  The Introspection Stuff

 ******************************************************/

#ifdef STACKLESS_SPY

static PyObject *
_peek(PyObject *self, PyObject *v)
{
	static PyObject *o;
	PyTypeObject *t;
	int i;

	if (v == Py_None) {
		return PyLong_FromLong((int)_peek);
	}
	if (PyCode_Check(v)) {
		return PyLong_FromLong((int)(((PyCodeObject*)v)->co_code));
	}
	if (PyLong_Check(v) && PyLong_AsLong(v) == 0) {
		return PyLong_FromLong((int)(&PyEval_EvalFrameEx_slp));
	}
	if (!PyLong_Check(v)) goto noobject;
	o = (PyObject*) PyLong_AsLong(v);
	/* this is plain heuristics, use for now */
	if (CANNOT_READ_MEM(o, sizeof(PyObject))) goto noobject;
	if (IS_ON_STACK(o)) goto noobject;
	if (o->ob_refcnt < 1 || o->ob_refcnt > 10000) goto noobject;
	t = o->ob_type;
	for (i=0; i<100; i++) {
		if (t == &PyType_Type)
			break;
		if (CANNOT_READ_MEM(t, sizeof(PyTypeObject))) goto noobject;
		if (IS_ON_STACK(o)) goto noobject;
		if (Py_REFCNT(t) < 1 || Py_REFCNT(t) > 10000) goto noobject;
		if (!(isalpha(t->tp_name[0])||t->tp_name[0]=='_'))
			goto noobject;
		t = Py_TYPE(t);
	}
	Py_INCREF(o);
	return o;
noobject:
	Py_INCREF(v);
	return v;
}

static char _peek__doc__[] =
"_peek(adr) -- try to find an object at adr.\n\
When no object is found, the address is returned.\n\
_peek(None)  _peek's address.\n\
_peek(0)     PyEval_EvalFrame's address.\n\
_peek(frame) frame's tstate address.\n\
Internal, considered dangerous!";

#endif

/* finding refcount problems */

#if defined(Py_TRACE_REFS)

static char _get_refinfo__doc__[] =
"_get_refinfo -- return (maximum referenced object,\n"
"refcount, ref_total, computed total)";

static PyObject *
_get_refinfo(PyObject *self)
{
	PyObject *op, *max = Py_None;
	PyObject *refchain;
	Py_ssize_t ref_total = _Py_RefTotal;
	Py_ssize_t computed_total = 0;

	refchain = PyTuple_New(0)->_ob_next; /* None doesn't work in 2.2 */
	Py_DECREF(refchain->_ob_prev);
	/* find real refchain */
	while (refchain->ob_type != NULL)
		refchain = refchain->_ob_next;

	for (op = refchain->_ob_next; op != refchain; op = op->_ob_next) {
        if (op->ob_refcnt > max->ob_refcnt)
		max = op;
		computed_total += op->ob_refcnt;
	}
	return Py_BuildValue("(Onnn)", max, max->ob_refcnt, ref_total,
			     computed_total);
}

static char _get_all_objects__doc__[] =
"_get_all_objects -- return a list with all objects but the list.";

static PyObject *
_get_all_objects(PyObject *self)
{
	PyObject *lis, *ob;
	lis = PyList_New(0);
	if (lis) {
		ob = lis->_ob_next;
		while (ob != lis && ob->ob_type != NULL) {
			if (PyList_Append(lis, ob))
				return NULL;
			ob = ob->_ob_next;
		}
	}
	return lis;
}

#endif

/******************************************************

  Special support for RPython

 ******************************************************/

/*
  In RPython, we are not allowed to use cyclic references
  without explicitly breaking them, or we leak memory.
  I wrote a tool to find out which code line causes
  unreachable objects. It works by running a full GC run
  after every line. To make this reasonably fast, it makes
  sense to remove the existing GC objects temporarily.
 */

static PyObject *
_gc_untrack(PyObject *self, PyObject *ob)
{
	PyObject_GC_UnTrack(ob);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
_gc_track(PyObject *self, PyObject *ob)
{
	PyObject_GC_Track(ob);
	Py_INCREF(Py_None);
	return Py_None;
}

static char _gc_untrack__doc__[] =
"_gc_untrack, gc_track -- remove or add an object from the gc list.";


/* List of functions defined in the module */

#define PCF PyCFunction
#define METH_KS METH_VARARGS | METH_KEYWORDS | METH_STACKLESS

static PyMethodDef stackless_methods[] = {
	{"schedule",		    (PCF)schedule,		METH_KS,
	 schedule__doc__},
	{"schedule_remove",	    (PCF)schedule_remove,	METH_KS,
	 schedule__doc__},
	{"run",			    (PCF)run_watchdog,		METH_VARARGS | METH_KEYWORDS,
	 run_watchdog__doc__},
	{"getruncount",		    (PCF)getruncount,		METH_NOARGS,
	 getruncount__doc__},
	{"getcurrent",		    (PCF)getcurrent,		METH_NOARGS,
	 getcurrent__doc__},
	{"getmain",		    (PCF)getmain,		METH_NOARGS,
	 getmain__doc__},
	{"enable_softswitch",	    (PCF)enable_softswitch,	METH_O,
	 enable_soft__doc__},
	{"test_cframe",		    (PCF)test_cframe,		METH_VARARGS | METH_KEYWORDS,
	 test_cframe__doc__},
	{"test_cframe_nr",	    (PCF)test_cframe_nr,	METH_VARARGS | METH_KEYWORDS,
	test_cframe_nr__doc__},
	{"test_outside",	    (PCF)test_outside,		METH_NOARGS,
	test_outside__doc__},
	{"test_cstate",		    (PCF)test_cstate,		METH_O,
	test_cstate__doc__},
	{"set_channel_callback",    (PCF)set_channel_callback,	METH_O,
	 set_channel_callback__doc__},
	{"set_schedule_callback",   (PCF)set_schedule_callback, METH_O,
	 set_schedule_callback__doc__},
	{"_pickle_moduledict",	    (PCF)slp_pickle_moduledict, METH_VARARGS,
	 slp_pickle_moduledict__doc__},
	{"get_thread_info",	    (PCF)get_thread_info,	METH_VARARGS,
	 get_thread_info__doc__},
	{"_gc_untrack",		    (PCF)_gc_untrack,		METH_O,
	_gc_untrack__doc__},
	{"_gc_track",		    (PCF)_gc_track,		METH_O,
	_gc_untrack__doc__},
#ifdef STACKLESS_SPY
	{"_peek",		    (PCF)_peek,			METH_O,
	 _peek__doc__},
#endif
#if defined(Py_TRACE_REFS)
	{"_get_refinfo",	    (PCF)_get_refinfo,		METH_NOARGS,
	 _get_refinfo__doc__},
	{"_get_all_objects",	    (PCF)_get_all_objects,	METH_NOARGS,
	 _get_all_objects__doc__},
#endif
	{"__reduce__",		    (PCF)slpmodule_reduce,	METH_NOARGS, NULL},
	{"__reduce_ex__",	    (PCF)slpmodule_reduce,	METH_VARARGS, NULL},
	{NULL,			    NULL}       /* sentinel */
};


PyDoc_STRVAR(stackless_doc,
"The Stackless module allows you to do multitasking without using threads.\n\
The essential objects are tasklets and channels.\n\
Please refer to their documentation.\n\
");

static PyTypeObject *PySlpModule_TypePtr;

/* this is a modified clone of PyModule_New */

static PyObject *
slpmodule_new(const char *name)
{
	PySlpModuleObject *m;
	PyObject *nameobj;
	m = PyObject_GC_New(PySlpModuleObject, PySlpModule_TypePtr);
	if (m == NULL)
		return NULL;
	m->md_def = NULL;
	m->md_state = NULL;
	m->__channel__ = NULL;
	m->__tasklet__ = NULL;
	nameobj = PyUnicode_FromString(name);
	m->md_dict = PyDict_New();
	if (m->md_dict == NULL || nameobj == NULL)
		goto fail;
	if (PyDict_SetItemString(m->md_dict, "__name__", nameobj) != 0)
		goto fail;
	if (PyDict_SetItemString(m->md_dict, "__doc__", Py_None) != 0)
		goto fail;
	if (PyDict_SetItemString(m->md_dict, "__package__", Py_None) != 0)
		goto fail;
	Py_DECREF(nameobj);
	PyObject_GC_Track(m);
	return (PyObject *)m;

 fail:
	Py_XDECREF(nameobj);
	Py_DECREF(m);
	return NULL;
}

static void
slpmodule_dealloc(PySlpModuleObject *m)
{
	PyObject_GC_UnTrack(m);
	if (m->md_dict != NULL) {
		_PyModule_Clear((PyObject *)m);
		Py_DECREF(m->md_dict);
	}
	Py_XDECREF(m->__channel__);
	Py_XDECREF(m->__tasklet__);
	Py_TYPE(m)->tp_free((PyObject *)m);
}


static PyTypeObject *
slpmodule_get__tasklet__(PySlpModuleObject *mod, void *context)
{
	Py_INCREF(mod->__tasklet__);
	return mod->__tasklet__;
}

static int
slpmodule_set__tasklet__(PySlpModuleObject *mod, PyTypeObject *type, void *context)
{
	if (!PyType_IsSubtype(type, &PyTasklet_Type))
		TYPE_ERROR("__tasklet__ must be a tasklet subtype", -1);
	Py_INCREF(type);
	Py_CLEAR(mod->__tasklet__);
	mod->__tasklet__ = type;
	return 0;
}

static PyTypeObject *
slpmodule_get__channel__(PySlpModuleObject *mod, void *context)
{
	Py_INCREF(mod->__channel__);
	return mod->__channel__;
}

static int
slpmodule_set__channel__(PySlpModuleObject *mod, PyTypeObject *type, void *context)
{
	if (!PyType_IsSubtype(type, &PyChannel_Type))
		TYPE_ERROR("__channel__ must be a channel subtype", -1);
	Py_INCREF(type);
	Py_CLEAR(mod->__channel__);
	mod->__channel__ = type;
	return 0;
}

static PyObject *
slpmodule_getdebug(PySlpModuleObject *mod)
{
#ifdef _DEBUG
	PyObject *ret = Py_True;
#else
	PyObject *ret = Py_False;
#endif
	Py_INCREF(ret);
	return ret;
}

static char uncollectables__doc__[] =
"Get a list of all tasklets which have a non-trivial C stack.\n\
These might need to be killed manually in order to free memory,\n\
since their C stack might prevend garbage collection.\n\
Note that a tasklet is reported for every C stacks it has.";

static PyObject *
slpmodule_getuncollectables(PySlpModuleObject *mod, void *context)
{
	PyObject *lis = PyList_New(0);
	PyCStackObject *cst = slp_cstack_chain;

	if (lis == NULL)
		return NULL;
	do {
		if (cst->task != NULL) {
			if (PyList_Append(lis, (PyObject *) cst->task)) {
				Py_DECREF(lis);
				return NULL;
			}
		}
		cst = cst->next;
	} while (cst != slp_cstack_chain);
	return lis;
}

static PyObject *
slpmodule_getthreads(PySlpModuleObject *mod, void *context)
{
	PyObject *lis = PyList_New(0);
	PyThreadState *ts = PyThreadState_GET();
	PyInterpreterState *interp = ts->interp;

	if (lis == NULL)
		return NULL;

	for (ts = interp->tstate_head; ts != NULL; ts = ts->next) {
		PyObject *id = PyLong_FromLong(ts->thread_id);

		if (id == NULL || PyList_Append(lis, id))
			return NULL;
	}
	PyList_Reverse(lis);
	return lis;
}


static PyGetSetDef slpmodule_getsetlist[] = {
	{"__tasklet__", (getter)slpmodule_get__tasklet__,
			(setter)slpmodule_set__tasklet__,
	 "The default tasklet type to be used.\n"
	 "It must be derived from the basic tasklet type."},
	{"__channel__", (getter)slpmodule_get__channel__,
			(setter)slpmodule_set__channel__,
	 "The default channel type to be used.\n"
	 "It must be derived from the basic channel type."},
	{"runcount",	(getter)getruncount, NULL,
	 "The number of currently runnable tasklets."},
	{"current",	(getter)getcurrent, NULL,
	 "The currently executing tasklet."},
	{"main",	(getter)getmain, NULL,
	 "The main tasklet of this thread."},
	{"debug",	(getter)slpmodule_getdebug, NULL,
	 "Flag telling whether this was a debug build."},
	{"uncollectables", (getter)slpmodule_getuncollectables, NULL,
	 uncollectables__doc__},
	{"threads", (getter)slpmodule_getthreads, NULL,
	 "a list of all thread ids, starting with main."},
	{0},
};

static char PySlpModule_Type__doc__[] =
"The stackless module has a special type derived from\n\
the module type, in order to be able to override some attributes.\n\
__tasklet__ and __channel__ are the default types\n\
to be used when these objects must be instantiated internally.\n\
runcount, current and main are attribute-like short-hands\n\
for the getruncount, getcurrent and getmain module functions.\n\
";

static PyTypeObject PySlpModule_TypeTemplate = {
	PyObject_HEAD_INIT(&PyType_Type)
	"slpmodule",
	sizeof(PySlpModuleObject),
	0,
	(destructor)slpmodule_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	0,				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
		Py_TPFLAGS_HAVE_GC,	/* tp_flags */
	PySlpModule_Type__doc__,	/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	slpmodule_getsetlist,		/* tp_getset */
	&PyModule_Type,			/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	PyObject_GC_Del,		/* tp_free */
};


int init_slpmoduletype(void)
{
	PyTypeObject *t = &PySlpModule_TypeTemplate;

	if ( (t = PyFlexType_Build(
		      "stackless", "slpmodule",t->tp_doc, t,
                       sizeof(PyFlexTypeObject), NULL) ) == NULL)
		return -1;
	PySlpModule_TypePtr = t;
	/* make sure that we cannot create any more instances */
	PySlpModule_TypePtr->tp_new = NULL;
	PySlpModule_TypePtr->tp_init = NULL;
	return 0;
}

static int init_stackless_methods(void)
{
	_stackless_method *p = _stackless_methtable;

	for (; p->type != NULL; p++) {
		PyTypeObject *t = p->type;
		size_t ind = p->offset & MFLAG_IND;
		size_t ofs = p->offset - ind;

		if (ind)
			t = *((PyTypeObject **)t);
		((signed char *) t)[ofs] = -1;
	}
	return 0;
}

/* this one must be called very early, before modules are used */

int
_PyStackless_InitTypes(void)
{
	if (0
	    || init_stackless_methods()
	    || init_cframetype()
	    || init_flextype()
	    || init_tasklettype()
	    || init_channeltype()
	    )
		return 0;
	return -1;
}


static struct PyModuleDef stacklessmodule = {
	PyModuleDef_HEAD_INIT,
	"stackless",
	stackless_doc,
	-1,
	stackless_methods,
	NULL,
	NULL,
	NULL,
	NULL,
	slpmodule_new
};

void
_PyStackless_Init(void)
{
	PyObject *dict;
	PyObject *modules;
	char *name = "stackless";
	PySlpModuleObject *m;

	if (init_slpmoduletype())
		return;

	/* record the thread state for thread support */
	slp_initial_tstate = PyThreadState_GET();

	/* Create the module and add the functions */
	slp_module = PyModule_Create(&stacklessmodule);
	if (slp_module == NULL)
		return; /* errors handled by caller */

	modules = PyImport_GetModuleDict();
	if (PyDict_SetItemString(modules, name, slp_module)) {
		Py_DECREF(slp_module);
		return;
	}
	Py_DECREF(slp_module); /* Yes, it still exists, in modules! */

	if (init_prickelpit()) return;

	dict = PyModule_GetDict(slp_module);

#define INSERT(name, object) \
	if (PyDict_SetItemString(dict, name, (PyObject*)object) < 0) return

	INSERT("slpmodule", PySlpModule_TypePtr);
	INSERT("cframe",    &PyCFrame_Type);
	INSERT("cstack",    &PyCStack_Type);
	INSERT("bomb",	    &PyBomb_Type);
	INSERT("tasklet",   &PyTasklet_Type);
	INSERT("channel",   &PyChannel_Type);
	INSERT("stackless", slp_module);

	m = (PySlpModuleObject *) slp_module;
	slpmodule_set__tasklet__(m, &PyTasklet_Type, NULL);
	slpmodule_set__channel__(m, &PyChannel_Type, NULL);
}

void
PyStackless_Fini(void)
{
	slp_scheduling_fini();
	slp_cstack_fini();
	slp_stacklesseval_fini();
}

#endif
