.. highlightlang:: c

.. _memoryview-objects:

.. index::
   object: memoryview

MemoryView objects
------------------

A :class:`memoryview` object exposes the C level :ref:`buffer interface
<bufferobjects>` as a Python object which can then be passed around like
any other object.


.. c:function:: PyObject *PyMemoryView_FromObject(PyObject *obj)

   Create a memoryview object from an object that provides the buffer interface.
   If *obj* supports writable buffer exports, the memoryview object will be
   readable and writable, other it will be read-only.


.. c:function:: PyObject *PyMemoryView_FromBuffer(Py_buffer *view)

   Create a memoryview object wrapping the given buffer structure *view*.
   The memoryview object then owns the buffer represented by *view*, which
   means you shouldn't try to call :c:func:`PyBuffer_Release` yourself: it
   will be done on deallocation of the memoryview object.


.. c:function:: PyObject *PyMemoryView_GetContiguous(PyObject *obj, int buffertype, char order)

   Create a memoryview object to a contiguous chunk of memory (in either
   'C' or 'F'ortran *order*) from an object that defines the buffer
   interface. If memory is contiguous, the memoryview object points to the
   original memory. Otherwise copy is made and the memoryview points to a
   new bytes object.


.. c:function:: int PyMemoryView_Check(PyObject *obj)

   Return true if the object *obj* is a memoryview object.  It is not
   currently allowed to create subclasses of :class:`memoryview`.


.. c:function:: Py_buffer *PyMemoryView_GET_BUFFER(PyObject *obj)

   Return a pointer to the buffer structure wrapped by the given
   memoryview object.  The object **must** be a memoryview instance;
   this macro doesn't check its type, you must do it yourself or you
   will risk crashes.

