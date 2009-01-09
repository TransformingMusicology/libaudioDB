#!/usr/bin/python

# Python code/library to query the audioDB via the SOAP web interface.
# by Malcolm Slaney, August/September 2008
# malcolm@ieee.org

import sys, socket
from xml.dom.minidom import parseString

global debug
debug = False
global dbName
dbName = 'tutorial.adb'

# From: http://www.informit.com/articles/article.aspx?p=686162&seqNum=2
#serverHost = 'research-hm3.corp.sk1.yahoo.com'
serverHost = 'localhost'
serverPort = 14476

# Start the server on serverHost with
#	./audioDB -s 14475

# Here are the templates used for the different kinds of queries.  We'll fill in the
# desired parameters are we go.
LIST_TEMPLATE = """
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
 xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
 xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
 xmlns:adb="http://tempuri.org/adb.xsd">
 <SOAP-ENV:Body>
  <adb:liszt>
   <dbName>%s</dbName>
   <lisztOffset>0</lisztOffset>
   <lisztLength>100000</lisztLength>
  </adb:liszt>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
"""

SEQUENCE_TEMPLATE = """
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
xmlns:xsd="http://www.w3.org/2001/XMLSchema"
xmlns:adb="http://tempuri.org/adb.xsd">
<SOAP-ENV:Body>
 <adb:sequenceQueryByKey>
  <dbName>%s</dbName>
  <featureFileName>%s</featureFileName>
  <queryType>%s</queryType>
  <trackFileName></trackFileName>
  <timesFileName></timesFileName>
  <queryPoint>%s</queryPoint>
  <pointNN>%s</pointNN>			<!-- Number of nearest neighbors to retrieve -->
  <trackNN>%s</trackNN>			<!-- Number of tracks to retrieve -->
  <sequenceLength>%s</sequenceLength>	<!-- Number of frames in a shingle -->
  <radius>%s</radius>			<!-- Distance radius to search -->
  <absolute-threshold>-4.0</absolute-threshold>
  <usingQueryPoint>1</usingQueryPoint>
  <lsh-exact>0</lsh-exact>
 </adb:sequenceQueryByKey>
</SOAP-ENV:Body>
</SOAP-ENV:Envelope>
"""

STATUS_TEMPLATE = """
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
 xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
 xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
 xmlns:adb="http://tempuri.org/adb.xsd">
 <SOAP-ENV:Body>
  <adb:status>
   <dbName>%s</dbName>
  </adb:status>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
"""

SHINGLE_QUERY_TEMPLATE = """
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
 xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
 xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
 xmlns:adb="http://tempuri.org/adb.xsd">
 <SOAP-ENV:Body>
  <adb:shingleQuery>
   <dbName>%s</dbName>
   <qVector>
   <dim>%s</dim>
      %s
      %s
   </qVector>
   <keyList></keyList>
   <timesFileName></timesFileName>
   <queryType>32</queryType>
   <queryPos>%s</queryPos>
   <pointNN>%s</pointNN>
   <trackNN>%s</trackNN>
   <sequenceLength>1</sequenceLength>
   <radius>%s</radius>
   <absolute-threshold>%s</absolute-threshold>
   <relative-threshold>%s</relative-threshold>
   <exhaustive>%s</exhaustive>
   <lsh-exact>%s</lsh-exact>
  </adb:shingleQuery>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
"""



FEATURE_QUERY_TEMPLATE = """
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
 xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
 xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
 xmlns:adb="http://tempuri.org/adb.xsd">
 <SOAP-ENV:Body>
  <adb:query>
   <dbName>%s</dbName>
   <qKey>%s</qKey>
   <keyList>%s</keyList>
   <timesFileName>%s</timesFileName>
   <powerFileName>%s</powerFileName>
   <qType>%s</qType>
   <qPos>%s</qPos>
   <pointNN>%s</pointNN>
   <segNN>%s</segNN>
   <segLen>%s</segLen>
   <radius>%s</radius>
   <absolute-threshold>%s</absolute-threshold>
   <relative-threshold>%s</relative-threshold>
   <exhaustive>%s</exhaustive>
   <lsh-exact>%s</lsh-exact>
   <no-unit-norming>%s</no-unit-norming>
  </adb:query>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
"""

###############  List Query - Show the files in the database ###########
# Return a list of (key identifier, frame length) pairs.
def RunListQuery():
	global debug, dbName
	message = LIST_TEMPLATE%(dbName)
	
	response = SendXMLCommand(message)
	return response

# Construct a list from the two DOM entries passed.  Used with the map routine to 
# assemble the output.
def ParseListConstruct(f,l):
	el = [f.firstChild.data.encode('latin-1'),l.firstChild.data.encode('latin-1')]
	return el
	
def ParseListXML(response):
	dom = parseString(response)
	fileElements = dom.getElementsByTagName('Rkey')
	# print fileElements
	lenElements = dom.getElementsByTagName('Rlen')
	# print lenElements
	return map(ParseListConstruct, fileElements, lenElements)

###############  Status Query - Show the status of the database ###########
# Return a dictionary with the status fields
def GetDomElement(dom, field):
	els = dom.getElementsByTagName(field)
	if len(els) > 0:
		return els[0].firstChild.data.encode('latin-1')
	else:
		return ""

def RunStatusQuery():
	global debug, dbName
	message = STATUS_TEMPLATE%(dbName)

	response = SendXMLCommand(message)
	# print response
	dom = parseString(response)
	status = {}
	status['numFiles'] = GetDomElement(dom, 'numFiles')
	status['dim'] = GetDomElement(dom, 'dim')
	status['length'] = GetDomElement(dom, 'length')
	status['dudCount'] = GetDomElement(dom, 'dudCount')
	status['nullCount'] = GetDomElement(dom, 'nullCount')
	return status



###############  Shingle/Matrix Query - Show the data closest to shingle range ###########
#
# Encode features as a matrix with dim columns, and (optionally) powers as a matrix with one column
# dim   # number of columns
# f1,1 f1,2...f1,dim # first row
# f2,1 f2,2...f2,dim
# ...
# fN,1 fN,2...fN,dim # last row
# p1,1         # first row's power
# p2,1
# ...
# pN,1         # last row's power
#
def RunShingleQuery():
	global debug, dbName
	featureDim = '3'
	queryVector='<v>1.0</v><v>0.5</v><v>0.25</v><v>0.5</v><v>1.5</v><v>1.0</v>'  # two rows of features
	powerVector='<p>-1.0</p><p>-1.0</p>'   # one power feature per row
	queryPos = '0'  # where in the feature sequence to start the shingle query
	pointNN = '10'  # how many near points to return per track
	trackNN = '10'  # how many near tracks to return
	radius = '1.0'  # search radius
	absoluteThreshold = '-4.5'  # absolute silence threshold in Bels (query and database shingles)
	relativeThreshold = '0'     # relative silence threshold in Bels between features, 0 = ignore
	exhaustive = '0'            # 1 = perform query using all subsequences of features of length sequenceLength
	lshExact = '0'              # if using an index then compute exact distances after LSH retrieval
	message = SHINGLE_QUERY_TEMPLATE
	message = SHINGLE_QUERY_TEMPLATE%(dbName, featureDim, queryVector, powerVector, queryPos, pointNN, trackNN, radius, absoluteThreshold, relativeThreshold, exhaustive, lshExact);
	# print message
	print message
	response = SendXMLCommand(message)
	ParseShingleXML(response)
		

###############  Sequence Query - Show the data closest to one query ###########
def RunSequenceQuery(argv):
	global debug, dbName	
	if len(argv) > 2:
		dbKey = argv[2]
		qType = '32'			# nSequence
		qPos = argv[3]
		pointNN = '10'
		trackNN = '5'
		seqLen = argv[4]
		queryRadius = '2'
	else:
		dbKey = 'tmp/3.chr'
		qType = '32'			# nSequence
		qPos = '110'
		pointNN = '10'
		trackNN = '5'
		seqLen = '20'
		queryRadius = '0.4'

	message = SEQUENCE_TEMPLATE
	message = SEQUENCE_TEMPLATE%(dbName, dbKey, qType, qPos, pointNN, trackNN, seqLen, queryRadius)
	# print message
	response = SendXMLCommand(message)
	ParseShingleXML(response)

###############  Sequence Query - Show the data closest to one query ###########
def RunQuery(argv):
#   <dbName>%s</dbName>
#   <qKey>%s</qKey>
#   <keyList>%s</keyList>
#   <timesFileName>%s</timesFileName>
#   <powerFileName>%s</powerFileName>
#   <qType>%s</qType>
#   <qPos>%s</qPos>
#   <pointNN>%s</pointNN>
#   <segNN>%s</segNN>
#   <segLen>%s</segLen>
#   <radius>%s</radius>
#   <absolute-threshold>%s</absolute-threshold>
#   <relative-threshold>%s</relative-threshold>
#   <exhaustive>%s</exhaustive>
#   <lsh-exact>%s</lsh-exact>
#   <no-unit-norming>%s</no-unit-norming>
	global debug, dbName
	if len(argv) > 2:
		featureFile = argv[2]
		powerFile = argv[3]
		qType = '32'			# nSequence
		qPos = argv[4]
		pointNN = '20'
		trackNN = '5'
		seqLen = argv[5]
		queryRadius = '0.2'
	else:
		featureFile = 'foo.chr12'
		powerFile = 'foo.power'
		qType = '32'			# nSequence
		qPos = '0'
		pointNN = '3'
		trackNN = '5'
		seqLen = '10'
		queryRadius = '0.2'

	message = FEATURE_QUERY_TEMPLATE
	message = FEATURE_QUERY_TEMPLATE%(dbName, featureFile, "", "", powerFile, qType, qPos, pointNN, trackNN, seqLen, queryRadius, '0.0', '0.0', '0', '1','0')

	print message
	response = SendXMLCommand(message)
	ParseShingleXML(response)

def ParseShingleXML(response):
	# Grab all the responses
	#	See http://diveintopython.org/xml_processing/parsing_xml.html
	dom = parseString(response)
	resultList = []
	for node in dom.getElementsByTagName('Rlist'):
		# print node.toxml()
		resultList.append(node.firstChild.data.encode('latin-1'))

	distanceList = []
	for node in dom.getElementsByTagName('Dist'):
		# print node.toxml()
		distanceList.append(node.firstChild.data.encode('latin-1'))

	positionList = []
	for node in dom.getElementsByTagName('Spos'):
		# print node.toxml()
		positionList.append(node.firstChild.data.encode('latin-1'))

	# print resultList
	# print distanceList
	# print positionList

	# Print out a summary of the most similar results
	for i in range(0,len(resultList)):
		if i > 0 and resultList[i] != resultList[i-1]:
			print
		print positionList[i], distanceList[i], resultList[i]

	dom.unlink()

###############  XML and Network Utilities ###########
# Send one XML SOAP command to the server.  Get back the response.

def SendXMLCommand(message):
	global debug
	if debug:
		print message
		print

	#Create a socket
	sSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	#Connect to server
	sSock.connect((serverHost, serverPort))

	#Send messages
	sSock.send(message)
	data = ""
	# Now loop, while getting all the data we can get from the socket.
	while True:
		c = sSock.recv(4096)
		if c == "":
			break
		data += c
	if data == "":
		print "No response from the audioDB server"
		sys.exit(0)
	# Split off the HTTP header and the data
	header,response = data.split("\r\n\r\n", 1)
	if debug:
		print 'Client received: ',response

	sSock.close()
	return response

	
###############  Main Program - Figure out which query we want ###########

# Argument processing scheme described at: http://docs.python.org/lib/module-getopt.html

import sys
if __name__=="__main__":
	cmdname = sys.argv[0]
	if len(sys.argv) == 1:
		print "Syntax: " + sys.argv[0] + " -{s,q,f,l} feature_file [power_file] pos len"
		sys.exit(1)

	queryType = sys.argv[1]
	if queryType == '-s' or queryType == 'status':
		response = RunStatusQuery()
		for k, v in response.iteritems():
			print k, v
	elif queryType == '-q' or queryType == 'query':
		RunSequenceQuery(sys.argv)
	elif queryType == '-f' or queryType == 'feature':
		RunQuery(sys.argv)
	elif queryType == '-l' or queryType == 'list':
		response = RunListQuery()
		# print response
		results = ParseListXML(response)
		for (f,l) in results:
			print "%s\t%s" % (f,l)
	elif queryType == '-v' or queryType == 'vector':
		response = RunShingleQuery()
		
		
