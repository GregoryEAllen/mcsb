%module(directors="1") MCSB
%include "cstring.i"
%include "exception.i"
%include "std_string.i"

%{
#include "MCSB/Client.h"
#include "MCSB/MessageSegment.h"
#include "MCSB/StdClientOptions.h"
#include "MCSB/MCSBVersion.h"

#if PY_VERSION_HEX < 0x02050000 // for python 2.4 on RHEL5
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

namespace MCSB {

void PyHandleMessage(const RecvMessageDescriptor& desc, void* arg)
{
	uint32_t msgID = desc.MessageID();
	size_t totalSize = desc.TotalSize();
	PyObject* buf = PyBuffer_New(totalSize);
	void* buffer;
	Py_ssize_t buffer_len;
	if (PyObject_AsWriteBuffer(buf,&buffer,&buffer_len)) {
		throw std::runtime_error("PyObject_AsWriteBuffer returned an error");
	}
	RecvMsgSegment* seg = static_cast<RecvMsgSegment*>(desc.Seg());
	seg->CopyToBuffer(buffer,totalSize);
	PyObject* arglist = Py_BuildValue("iO",msgID,buf);
	PyObject* result = PyEval_CallObject((PyObject *)arg, arglist);
	Py_DECREF(arglist);
	Py_DECREF(buf);
	
	if (result) {
		Py_DECREF(result);
	} else {
		PyErr_PrintEx(1);
		throw std::runtime_error("PyHandleMessage CallObject returned error");
	}
}

bool PyMethodsEqual(void* arg1, void* arg2)
// compare two PyMethods and return true if they are equal
{
	if (arg1==arg2) return 1;
	if (!arg1 || !arg2) return 0;
	PyObject* pyobj1 = (PyObject*)arg1;
	PyObject* pyobj2 = (PyObject*)arg2;
	if (!PyMethod_Check(pyobj1) || !PyMethod_Check(pyobj2)) return 0;
	if (PyMethod_Function(pyobj1)!=PyMethod_Function(pyobj2)) return 0;
	if (PyMethod_Self(pyobj1)!=PyMethod_Self(pyobj2)) return 0;
	if (PyMethod_Class(pyobj1)!=PyMethod_Class(pyobj2)) return 0;
	return 1;
}

} // namespace MCSB
%}

%apply unsigned int { uint32_t }

// send C++ exceptions up to Python
%exception {
	try { $action }
	catch (const Swig::DirectorException &e) { SWIG_fail; }
	catch (const std::runtime_error &ex) {
	 	PyErr_SetString(PyExc_RuntimeError,ex.what());
		return NULL;
	}
}

// handle exceptions that occurred in Python
%feature("director:except") {
	PyRun_SimpleString("import traceback; traceback.print_exc()");
	exit(-1);
}

// these are the classes and functions that will be compiled to python
%ignore MCSB::ClientOptions::ClientOptions(int,char *const []);
%ignore MCSB::ClientOptions::kCrcStrs; // because it's private
%include "MCSB/ClientOptions.h"
%include "MCSB/StdClientOptions.h"
%ignore dbprinter;
%include "MCSB/dbprinter.h"

%ignore MessageHandlerThunk;
%ignore DropReportHandlerThunk;
%ignore ConnectionChangeHandlerThunk;
%include "MCSB/ClientCallbacks.h"

%ignore MCSB::SendMessageDescriptor::operator=;
%ignore MCSB::RecvMessageDescriptor::operator=;
%rename(AsSendMessageDescriptorRef) MCSB::SendMessageDescriptor::operator SendMessageDescriptorRef;
%include "MCSB/MessageDescriptors.h"

%ignore MCSB::BaseClient::GetSendMessageDescriptor;
%ignore MCSB::BaseClient::SendMessage(uint32_t,void const *,uint32_t);
%ignore MCSB::BaseClient::SendMessage(uint32_t,struct iovec const [],int);
%ignore MCSB::BaseClient::SendMessage(uint32_t,SendMessageDescriptor&,uint32_t);
%include "MCSB/BaseClient.h"
%extend MCSB::BaseClient {
	int fileno(void) const {
		return self->FD();
	}
	// SendString deprecated, for MCSBv1 compatibility
	int SendString(uint32_t msgID, const std::string& msg) {
		return self->SendMessage(msgID,msg.data(),msg.size());
	}
	int SendMessage(uint32_t msgID, PyObject *pyobj) {
		if (PyString_Check(pyobj)) {
			uint32_t len = PyString_Size(pyobj);
			const char* str = PyString_AsString(pyobj);
			return self->SendMessage(msgID,str,len);
		}
		if (PyBuffer_Check(pyobj)) {
			const void* buffer;
			Py_ssize_t buffer_len;
			if (PyObject_AsReadBuffer(pyobj,&buffer,&buffer_len)) {
				throw std::runtime_error("PyObject_AsReadBuffer returned an error");
			}
			return self->SendMessage(msgID,buffer,buffer_len);
		}
		throw std::runtime_error("SendMessage passed invalid object type");
	}
}

%ignore MCSB::Client::RegisterForMsgID(uint32_t,MessageHandlerFunction);
%ignore MCSB::Client::RegisterForMsgID(uint32_t,MessageHandlerFunction,void*);
%ignore MCSB::Client::RegisterForMsgID(uint32_t, T*);
%ignore MCSB::Client::DeregisterForMsgID(uint32_t,MessageHandlerFunction);
%ignore MCSB::Client::DeregisterForMsgID(uint32_t,MessageHandlerFunction,void*);
%ignore MCSB::Client::DeregisterForMsgID(uint32_t,MessageHandlerFunction,void*,bool (*)(void *,void *));
%ignore MCSB::Client::DeregisterForMsgID(uint32_t, T*);
%include "MCSB/Client.h"
%extend MCSB::Client {
	void RegisterForMsgID(uint32_t msgID, PyObject *pyfunc) {
		if (!PyCallable_Check(pyfunc))
			throw std::runtime_error("RegisterForMsgID passed invalid object type");
		self->RegisterForMsgID(msgID, &MCSB::PyHandleMessage, (void*)pyfunc);
		Py_INCREF(pyfunc);
	}
	bool DeregisterForMsgID(uint32_t msgID, PyObject *pyfunc) {
		bool result = self->DeregisterForMsgID(msgID, &MCSB::PyHandleMessage, (void*)pyfunc);
		if (!result && PyMethod_Check(pyfunc)) {
			PyObject* mapfunc = (PyObject*)self->DeregisterForMsgID(msgID, &MCSB::PyHandleMessage,
				(void*)pyfunc, &MCSB::PyMethodsEqual);
			result = !!mapfunc;
			pyfunc = mapfunc; // want to decref mapfunc (it was incref'd)
		}
		if (result) {
			Py_DECREF(pyfunc);
		}
		return result;
	}
}

%constant unsigned MAJOR_VERSION = MCSB_MAJOR_VERSION;
%constant unsigned MINOR_VERSION = MCSB_MINOR_VERSION;
%constant unsigned PATCH_VERSION = MCSB_PATCH_VERSION;
%constant char* VERSION = MCSB_VERSION;
%constant unsigned VERSION_HEX = MCSB_VERSION_HEX;
