from MCSB import *
import sys, os

try:
	import argparse
except ImportError, ex:
	pass
	#print ex

try:
	import optparse
except ImportError, ex:
	pass
	#print ex

class ClientOptionParser:
	def __init__(self,parser=None,shortopts=1):
		if parser is None:
			if "argparse" in sys.modules:
				formatter = argparse.ArgumentDefaultsHelpFormatter
				parser = argparse.ArgumentParser(formatter_class=formatter)
			elif "optparse" in sys.modules:
				parser = optparse.OptionParser()
		self.parser = parser
		
		if "argparse" in sys.modules and parser.__class__==argparse.ArgumentParser:
			stdgroup = parser.add_argument_group("standard MCSB Client arguments")
			advgroup = parser.add_argument_group("advanced MCSB Client arguments")

		elif "optparse" in sys.modules and parser.__class__==optparse.OptionParser:
			group = optparse.OptionGroup(parser,"standard MCSB Client arguments")
			stdgroup = parser.add_option_group(group)
			stdgroup.add_argument = stdgroup.add_option
			group = optparse.OptionGroup(parser,"advanced MCSB Client arguments")
			advgroup = parser.add_option_group(group)
			advgroup.add_argument = advgroup.add_option

		args = ["--"+kLOpt_minProducerBytes]
		if shortopts: args.insert(0, "-b")
		kwargs = {"dest":"minProducerBytes", "type":int, "metavar":"BYTES",
			"help":"Client minimum producer bytes"}
		stdgroup.add_argument(*args,**kwargs)
		self.DefaultMinProducerBytes()

		args = ["--"+kLOpt_minConsumerBytes]
		if shortopts: args.insert(0, "-B")
		kwargs = {"dest":"minConsumerBytes", "type":int, "metavar":"BYTES",
			"help":"Client minimum consumer bytes"}
		stdgroup.add_argument(*args,**kwargs)
		self.DefaultMinConsumerBytes()

		args = ["--"+kLOpt_ctrlSockName]
		if shortopts: args.insert(0, "-c")
		kwargs = {"dest":"ctrlSockName", "metavar":"NAME",
			"help":"Control socket name"}
		stdgroup.add_argument(*args,**kwargs)
		self.DefaultCtrlSockName()

		args = ["--"+kLOpt_minProducerSlabs]
		if shortopts>1: args.insert(0, "-s")
		kwargs = {"dest":"minProducerSlabs", "type":int, "metavar":"SLABS",
			"help":"Client minimum producer slabs"}
		advgroup.add_argument(*args,**kwargs)
		self.DefaultMinProducerSlabs()

		args = ["--"+kLOpt_minConsumerSlabs]
		if shortopts>1: args.insert(0, "-S")
		kwargs = {"dest":"minConsumerSlabs","type":int, "metavar":"SLABS",
			"help":"Client minimum consumer slabs"}
		advgroup.add_argument(*args,**kwargs)
		self.DefaultMinConsumerSlabs()

		args = ["--"+kLOpt_clientName]
		if shortopts>1: args.insert(0, "-n")
		kwargs = {"dest":"clientName", "metavar":"NAME", "help":"Client name"}
		advgroup.add_argument(*args,**kwargs)
		self.DefaultClientName()

		args = ["--"+kLOpt_crcPolicy]
		if shortopts>1: args.insert(0, "-p")
		kwargs = {"dest":"crcPolicy", "choices":self.CrcPolicyChoices(),
			"metavar":"POLICY", "help":"Client CRC policy"}
		advgroup.add_argument(*args,**kwargs)
		self.DefaultCrcPolicy()

		kwargs = {"dest":"verbosity", "type":int, "help":"Client verbosity"}
		advgroup.add_argument("--"+kLOpt_verbosity,**kwargs)
		self.DefaultVerbosity()

	def DefaultMinProducerBytes(self,bytes=None):
		if bytes is None:
			default = int(os.getenv(kEnv_minProducerBytes, ClientOptions.kDefaultMinProducerBytes))
		else:
			default = bytes
		self.parser.set_defaults(minProducerBytes=default)

	def DefaultMinConsumerBytes(self,bytes=None):
		if bytes is None:
			default = int(os.getenv(kEnv_minConsumerBytes, ClientOptions.kDefaultMinConsumerBytes))
		else:
			default = bytes
		self.parser.set_defaults(minConsumerBytes=default)

	def DefaultMinProducerSlabs(self,slabs=None):
		if slabs is None:
			default = int(os.getenv(kEnv_minProducerSlabs, ClientOptions.kDefaultMinProducerSlabs))
		else:
			default = slabs
		self.parser.set_defaults(minProducerSlabs=default)

	def DefaultMinConsumerSlabs(self,slabs=None):
		if slabs is None:
			default = int(os.getenv(kEnv_minConsumerSlabs, ClientOptions.kDefaultMinConsumerSlabs))
		else:
			default = slabs
		self.parser.set_defaults(minConsumerSlabs=default)

	def DefaultCtrlSockName(self,name=None):
		if name is None:
			default = os.getenv(kEnv_ctrlSockName)
			if default is None:
				opts = ClientOptions(sys.argv[0])
				default = opts.ctrlSockName
		else:
			default = name
		default = ClientOptions.SubstituteUsername(default)
		self.parser.set_defaults(ctrlSockName=default)

	def DefaultClientName(self,name=None):
		if name is None:
			default = os.getenv(kEnv_clientName)
			if default is None:
				opts = ClientOptions(sys.argv[0])
				default = opts.clientName
		else:
			default = name
		self.parser.set_defaults(clientName=default)

	def DefaultCrcPolicy(self,policy=None):
		if policy is None:
			default = os.getenv(kEnv_crcPolicy, ClientOptions.DefaultCrcStr())
		else:
			default = policy
		self.parser.set_defaults(crcPolicy=default)

	def DefaultVerbosity(self,verbosity=None):
		if verbosity is None:
			default = int(os.getenv(kEnv_verbosity, ClientOptions.kDefaultVerbosity))
		else:
			default = verbosity
		self.parser.set_defaults(verbosity=default)

	@staticmethod
	def CrcPolicyChoices():
		choices = [ ClientOptions.eNoCrcs, ClientOptions.eSetCrcs,
			ClientOptions.eVerifyCrcs, ClientOptions.eSetAndVerifyCrcs,
			ClientOptions.eDefaultCrcStr ]
		return [ ClientOptions.CrcPolicyStr(choice) for choice in choices ]

	def GetClientOptions(self):
		if "argparse" in sys.modules and self.parser.__class__==argparse.ArgumentParser:
			args = self.parser.parse_args()
		elif "optparse" in sys.modules and self.parser.__class__==optparse.OptionParser:
			(args, pargs) = self.parser.parse_args()
		return ClientOptionsFromParsedArgs(args)

def ClientOptionsFromParsedArgs(args):
	opts = ClientOptions()
	opts.minProducerBytes = args.minProducerBytes
	opts.minConsumerBytes = args.minConsumerBytes
	opts.minProducerSlabs = args.minProducerSlabs
	opts.minConsumerSlabs = args.minConsumerSlabs
	opts.ctrlSockName = args.ctrlSockName
	opts.clientName = args.clientName
	opts.SetCrcPolicy(args.crcPolicy)
	opts.verbosity = args.verbosity
	return opts

def ArgParseClientOptions(args=None):
	if args is None:
		# nothing was passed in, all up to us
		fc = argparse.ArgumentDefaultsHelpFormatter
		argParser = argparse.ArgumentParser(formatter_class=fc)
		parser = ClientOptionParser(argParser)
		args = argParser.parse_args()
	return ClientOptionsFromParsedArgs(args)

def OptParseClientOptions(opts=None):
	if opts is None:
		# nothing was passed in, all up to us
		optParser = optparse.OptionParser()
		parser = ClientOptionParser(optParser)
		(opts, args) = optParser.parse_args()
	return ClientOptionsFromParsedArgs(opts)
