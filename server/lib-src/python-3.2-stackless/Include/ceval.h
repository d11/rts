#ifndef Py_CEVAL_H
#define Py_CEVAL_H
#ifdef __cplusplus
extern "C" {
#endif


/* Interface to random parts in ceval.c */

PyAPI_FUNC(PyObject *) PyEval_CallObjectWithKeywords(
    PyObject *, PyObject *, PyObject *);

/* Inline this */
#define PyEval_CallObject(func,arg) \
    PyEval_CallObjectWithKeywords(func, arg, (PyObject *)NULL)

PyAPI_FUNC(PyObject *) PyEval_CallFunction(PyObject *obj,
                                           const char *format, ...);
PyAPI_FUNC(PyObject *) PyEval_CallMethod(PyObject *obj,
                                         const char *methodname,
                                         const char *format, ...);

#ifndef Py_LIMITED_API
PyAPI_FUNC(void) PyEval_SetProfile(Py_tracefunc, PyObject *);
PyAPI_FUNC(void) PyEval_SetTrace(Py_tracefunc, PyObject *);
#endif

struct _frame; /* Avoid including frameobject.h */

PyAPI_FUNC(PyObject *) PyEval_GetBuiltins(void);
PyAPI_FUNC(PyObject *) PyEval_GetGlobals(void);
PyAPI_FUNC(PyObject *) PyEval_GetLocals(void);
PyAPI_FUNC(struct _frame *) PyEval_GetFrame(void);

/* Look at the current frame's (if any) code's co_flags, and turn on
   the corresponding compiler flags in cf->cf_flags.  Return 1 if any
   flag was set, else return 0. */
#ifndef Py_LIMITED_API
PyAPI_FUNC(int) PyEval_MergeCompilerFlags(PyCompilerFlags *cf);
#endif

PyAPI_FUNC(int) Py_AddPendingCall(int (*func)(void *), void *arg);
PyAPI_FUNC(int) Py_MakePendingCalls(void);

/* Protection against deeply nested recursive calls

   In Python 3.0, this protection has two levels:
   * normal anti-recursion protection is triggered when the recursion level
     exceeds the current recursion limit. It raises a RuntimeError, and sets
     the "overflowed" flag in the thread state structure. This flag
     temporarily *disables* the normal protection; this allows cleanup code
     to potentially outgrow the recursion limit while processing the
     RuntimeError.
   * "last chance" anti-recursion protection is triggered when the recursion
     level exceeds "current recursion limit + 50". By construction, this
     protection can only be triggered when the "overflowed" flag is set. It
     means the cleanup code has itself gone into an infinite loop, or the
     RuntimeError has been mistakingly ignored. When this protection is
     triggered, the interpreter aborts with a Fatal Error.

   In addition, the "overflowed" flag is automatically reset when the
   recursion level drops below "current recursion limit - 50". This heuristic
   is meant to ensure that the normal anti-recursion protection doesn't get
   disabled too long.

   Please note: this scheme has its own limitations. See:
   http://mail.python.org/pipermail/python-dev/2008-August/082106.html
   for some observations.
*/
PyAPI_FUNC(void) Py_SetRecursionLimit(int);
PyAPI_FUNC(int) Py_GetRecursionLimit(void);

#define Py_EnterRecursiveCall(where)  \
            (_Py_MakeRecCheck(PyThreadState_GET()->recursion_depth) &&  \
             _Py_CheckRecursiveCall(where))
#define Py_LeaveRecursiveCall()                         \
    do{ if(_Py_MakeEndRecCheck(PyThreadState_GET()->recursion_depth))  \
      PyThreadState_GET()->overflowed = 0;  \
    } while(0)
PyAPI_FUNC(int) _Py_CheckRecursiveCall(char *where);
PyAPI_DATA(int) _Py_CheckRecursionLimit;

#ifdef USE_STACKCHECK
/* With USE_STACKCHECK, we artificially decrement the recursion limit in order
   to trigger regular stack checks in _Py_CheckRecursiveCall(), except if
   the "overflowed" flag is set, in which case we need the true value
   of _Py_CheckRecursionLimit for _Py_MakeEndRecCheck() to function properly.
*/
#  define _Py_MakeRecCheck(x)  \
    (++(x) > (_Py_CheckRecursionLimit += PyThreadState_GET()->overflowed - 1))
#else
#  define _Py_MakeRecCheck(x)  (++(x) > _Py_CheckRecursionLimit)
#endif

#define _Py_MakeEndRecCheck(x) \
    (--(x) < ((_Py_CheckRecursionLimit > 100) \
        ? (_Py_CheckRecursionLimit - 50) \
        : (3 * (_Py_CheckRecursionLimit >> 2))))

#define Py_ALLOW_RECURSION \
  do { unsigned char _old = PyThreadState_GET()->recursion_critical;\
    PyThreadState_GET()->recursion_critical = 1;

#define Py_END_ALLOW_RECURSION \
    PyThreadState_GET()->recursion_critical = _old; \
  } while(0);

PyAPI_FUNC(const char *) PyEval_GetFuncName(PyObject *);
PyAPI_FUNC(const char *) PyEval_GetFuncDesc(PyObject *);

PyAPI_FUNC(PyObject *) PyEval_GetCallStats(PyObject *);
PyAPI_FUNC(PyObject *) PyEval_EvalFrame(struct _frame *);
PyAPI_FUNC(PyObject *) PyEval_EvalFrameEx(struct _frame *f, int exc);

#ifdef STACKLESS
PyAPI_FUNC(PyObject *) PyEval_EvalFrame_slp(struct _frame *f, int exc, PyObject *retval);
PyAPI_FUNC(PyObject *) PyEval_EvalFrameEx_slp(struct _frame *f, int exc, PyObject *retval);
#endif
/* Interface for threads.

   A module that plans to do a blocking system call (or something else
   that lasts a long time and doesn't touch Python data) can allow other
   threads to run as follows:

    ...preparations here...
    Py_BEGIN_ALLOW_THREADS
    ...blocking system call here...
    Py_END_ALLOW_THREADS
    ...interpret result here...

   The Py_BEGIN_ALLOW_THREADS/Py_END_ALLOW_THREADS pair expands to a
   {}-surrounded block.
   To leave the block in the middle (e.g., with return), you must insert
   a line containing Py_BLOCK_THREADS before the return, e.g.

    if (...premature_exit...) {
        Py_BLOCK_THREADS
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

   An alternative is:

    Py_BLOCK_THREADS
    if (...premature_exit...) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    Py_UNBLOCK_THREADS

   For convenience, that the value of 'errno' is restored across
   Py_END_ALLOW_THREADS and Py_BLOCK_THREADS.

   WARNING: NEVER NEST CALLS TO Py_BEGIN_ALLOW_THREADS AND
   Py_END_ALLOW_THREADS!!!

   The function PyEval_InitThreads() should be called only from
   init_thread() in "_threadmodule.c".

   Note that not yet all candidates have been converted to use this
   mechanism!
*/

PyAPI_FUNC(PyThreadState *) PyEval_SaveThread(void);
PyAPI_FUNC(void) PyEval_RestoreThread(PyThreadState *);

#ifdef WITH_THREAD

PyAPI_FUNC(int)  PyEval_ThreadsInitialized(void);
PyAPI_FUNC(void) PyEval_InitThreads(void);
PyAPI_FUNC(void) _PyEval_FiniThreads(void);
PyAPI_FUNC(void) PyEval_AcquireLock(void);
PyAPI_FUNC(void) PyEval_ReleaseLock(void);
PyAPI_FUNC(void) PyEval_AcquireThread(PyThreadState *tstate);
PyAPI_FUNC(void) PyEval_ReleaseThread(PyThreadState *tstate);
PyAPI_FUNC(void) PyEval_ReInitThreads(void);

#ifndef Py_LIMITED_API
PyAPI_FUNC(void) _PyEval_SetSwitchInterval(unsigned long microseconds);
PyAPI_FUNC(unsigned long) _PyEval_GetSwitchInterval(void);
#endif

#define Py_BEGIN_ALLOW_THREADS { \
                        PyThreadState *_save; \
                        _save = PyEval_SaveThread();
#define Py_BLOCK_THREADS        PyEval_RestoreThread(_save);
#define Py_UNBLOCK_THREADS      _save = PyEval_SaveThread();
#define Py_END_ALLOW_THREADS    PyEval_RestoreThread(_save); \
                 }

#else /* !WITH_THREAD */

#define Py_BEGIN_ALLOW_THREADS {
#define Py_BLOCK_THREADS
#define Py_UNBLOCK_THREADS
#define Py_END_ALLOW_THREADS }

#endif /* !WITH_THREAD */

#ifndef Py_LIMITED_API
PyAPI_FUNC(int) _PyEval_SliceIndex(PyObject *, Py_ssize_t *);
PyAPI_FUNC(void) _PyEval_SignalAsyncExc(void);
#endif


#ifdef __cplusplus
}
#endif
#endif /* !Py_CEVAL_H */
