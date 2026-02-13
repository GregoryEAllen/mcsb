import PythonPaths
import os, sys, subprocess, signal, time

try:
	import argparse
except ImportError, ex:
	print ex

try:
	import optparse
except ImportError, ex:
	print ex

import MCSB

class Manager:
	def __init__(self, opts):
		MCSBManager = PythonPaths.MCSBManager_Location
		args = [MCSBManager, '-vfF']
		args.append('-c%s'%opts.ctrlSockName)
		self.manager = subprocess.Popen(args)
		# give the Manager some time to start up
		for t in xrange(100):
			time.sleep(.1)
			if os.path.exists(opts.ctrlSockName):
				break
	def __del__(self):
		print "terminating MCSBManager"
		#self.manager.terminate() # only in python 2.6 and later
		os.kill(self.manager.pid,signal.SIGTERM)
		self.manager.wait()

def test1(opts):
	print 'test1'
	client = MCSB.Client(opts)

	try:
		client.SendMessage(0)
		sys.exit(-1)
	except RuntimeError,ex:
		print 'RuntimeError:', ex
	except TypeError,ex:
		print 'TypeError:', ex

	try:
		client.RegisterForMsgID(0,1)
		sys.exit(-1)
	except RuntimeError,ex:
		print 'RuntimeError:', ex

	try:
		client.RegisterForMsgID(0,"foo")
		sys.exit(-1)
	except RuntimeError,ex:
		print 'RuntimeError:', ex

	try:
		client.DeregisterForMsgID(0,"foo",0,0)
		sys.exit(-1)
	except RuntimeError,ex:
		print 'RuntimeError:', ex
	except TypeError,ex:
		print 'TypeError:', ex
	
	return 0

count = 0
def PyFunction(msgID, buf):
	global count
	print sys._getframe().f_code.co_name
	print "%d %s %d" % (msgID, buf, count)
	count += 1

class PyClass:
	def __init__(self):
		self.count = 0
	def PyMethod(self, msgID, buf):
		print sys._getframe().f_code.co_name
		print "%d %s %d" % (msgID, buf, self.count)
		self.count += 1

class PyCallable:
	def __init__(self):
		self.count = 0
	def __call__(self, msgID, buf):
		print sys._getframe().f_code.co_name
		print "%d %s %d" % (msgID, buf, self.count)
		self.count += 1

def test2(opts):
	print 'test2'
	client = MCSB.Client(opts)

	client.RegisterForMsgID(0,PyFunction)
	pyClass = PyClass()
	client.RegisterForMsgID(0,pyClass.PyMethod)
	pyCallable = PyCallable()
	client.RegisterForMsgID(0,pyCallable)
	pyClass2 = PyClass()
	pyMethodObj = pyClass2.PyMethod
	client.RegisterForMsgID(0,pyMethodObj)

	while (client.MaxSendMessageSize()==0):
		client.Poll(1)

	#client.SendString(0,"test") deprecated
	client.SendMessage(0,"testA")
	client.SendMessage(0,"testB")
	client.SendMessage(0,"testC")

	for x in xrange(10):
		client.Poll(.01)

	client.Close()
	client.Poll(.01)

	client.SendMessage(0,"testD")
	client.SendMessage(0,"testE")

	for x in xrange(10):
		client.Poll(.01)

	if not client.DeregisterForMsgID(0,PyFunction):
		raise RuntimeError("DeregisterForMsgID failed")
	if not client.DeregisterForMsgID(0,pyClass.PyMethod):
		raise RuntimeError("DeregisterForMsgID failed")
	if not client.DeregisterForMsgID(0,pyCallable):
		raise RuntimeError("DeregisterForMsgID failed")
	if not client.DeregisterForMsgID(0,pyMethodObj):
		raise RuntimeError("DeregisterForMsgID failed")

	client.SendMessage(0,"testF") # shouldn't get more
	for x in xrange(10):
		client.Poll(.01)

	if (count!=pyClass.count or count!=pyCallable.count or count!=pyClass2.count):
		raise RuntimeError("count mismatch")
	expectedCount = 5
	if (count!=expectedCount):
		raise RuntimeError("count %d, expectedCount %d"%(count,expectedCount))

	maxSendSize = client.MaxSendMessageSize()
	maxRecvSize = client.MaxRecvMessageSize()
	maxSize = max(maxSendSize,maxRecvSize)
	print 'maxSize %d' % maxSize
	
	return 0

def PyFunction2(msgID, buf):
	print sys._getframe().f_code.co_name
	print "%d %s %d" % (msgID, buf, count)
	buf[4] = 'G' # FIXME - this should be read-only?
#	client.DeregisterForMsgID(0,PyFunction2)
#	client.SendMessage(msgID, buf)

def test3(opts):
	print sys._getframe().f_code.co_name
	client = MCSB.Client(opts)
	client.RegisterForMsgID(0,PyFunction2)
	client.SendMessage(0,"testF")
	for x in xrange(10):
		client.Poll(.01)
	return 0

def test4():
	print sys._getframe().f_code.co_name

	# this uses argparse if available, else optparse
	opts = MCSB.ClientOptionParser().GetClientOptions()
	opts.Print()

	if 'argparse' in sys.modules:
		# short form, specifically using argparse
		opts = MCSB.ArgParseClientOptions()
		opts.Print()
		# long form, with full access to argparse
		argParser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
		parser = MCSB.ClientOptionParser(argParser)
		args = argParser.parse_args()
		opts = MCSB.ClientOptionsFromParsedArgs(args)
		opts.Print()

	if 'optparse' in sys.modules:
		# short form, specifically using optparse
		opts = MCSB.OptParseClientOptions()
		opts.Print()
		# long form, with full access to optparse
		optParser = optparse.OptionParser()
		parser = MCSB.ClientOptionParser(optParser)
		(opts, args) = optParser.parse_args()
		opts = MCSB.ClientOptionsFromParsedArgs(opts)
		opts.Print()

	return 0

def test5():
	print "MCSB.MAJOR_VERSION %d" % MCSB.MAJOR_VERSION
	print "MCSB.MINOR_VERSION %d" % MCSB.MINOR_VERSION
	print "MCSB.PATCH_VERSION %d" % MCSB.PATCH_VERSION
	print "MCSB.VERSION %s" % MCSB.VERSION
	print "MCSB.VERSION_HEX 0x%X" % MCSB.VERSION_HEX
	print "MCSB Version %s, HgRevision %s" % (MCSB.GetVersion(), MCSB.GetHgRevision())
	return 0;

def main():
	parser = MCSB.ClientOptionParser()
	parser.DefaultCtrlSockName("/tmp/mcsb-%U-test.sock")
	opts = parser.GetClientOptions()
	opts.Print()

	manager = Manager(opts)

	result = test1(opts)
	if result: return result
	result = test2(opts)
	if result: return result
	result = test3(opts)
	if result: return result
	result = test4()
	if result: return result
	result = test5()
	if ~result:
		print '==== PASS ===='
	
	return result

if __name__ == '__main__':
	sys.exit(main())
