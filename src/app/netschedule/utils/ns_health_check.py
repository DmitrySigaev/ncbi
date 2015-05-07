#!/opt/python-2.7/bin/python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server health check script
"""

import sys, os, cgi, datetime, socket, re
from distutils.version import StrictVersion
from optparse import OptionParser
from random import randrange
from time import sleep


# Defines three script modes
class ScriptMode:
    LBSMD222_OLD_CLIENT = 1
    LBSMD222 = 2


SCRIPT_MODE = ScriptMode.LBSMD222

MAX_OLD_MODE_VALUE = 99

BASE_RESERVE_CODE = 100
BASE_DOWN_CODE = 111
BASE_NO_ACTION_ALERT_CODE = 211
BASE_STANDBY_CODE = 200
NO_CHANGE_CODE = 123


VERBOSE = False
COMMUNICATION_TIMEOUT = 1
DYNAMIC_QUEUE_TO_TEST = "LBSMDTestQueue"
CLIENT_NODE = "health_check"
OTHER_CLIENT_DELAY = 1.0


MEMORY_LIMIT = 90       # Percentage of used
FD_LIMIT = 50           # At least 50 fds must be still available

PENALTY_CURVE = [ 90, 99 ]


# Set the value to None and no log file operations will be done
VERBOSE_LOG_FILE = None
#VERBOSE_LOG_FILE = "/home/satskyse/ns_healthcheck_log/healthcheck.log"


LOG_FILE = None
if VERBOSE_LOG_FILE is not None:
    try:
        LOG_FILE = open( VERBOSE_LOG_FILE, "a" )
    except:
        LOG_FILE = None



def printVerbose( msg ):
    " Prints stdout message conditionally "
    timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
    if LOG_FILE is not None:
        try:
            LOG_FILE.write( timestamp + " " + msg + "\n" )
            LOG_FILE.flush()
        except:
            pass

    if VERBOSE:
        print timestamp + " " + msg
    return

def printStderr( msg ):
    " Prints onto stderr with a prefix "
    timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
    print >> sys.stderr, timestamp + " NetSchedule check script. " + msg
    return

class UnexpectedNSResponse( Exception ):
    " NetSchedule produces trash "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSError( Exception ):
    " NetSchedule answer has ERR:... "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSShuttingDown( Exception ):
    " NetSchedule is shutting down "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSStaticCheckError( Exception ):
    " Memory or FD are consumed above limits "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSAccessDenied( Exception ):
    " No permissions for commands "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSDirectConnect:
    " Serves the direct connection to the server "
    def __init__( self, connPoint ):
        parts = connPoint.split( ":" )
        self.__host = parts[ 0 ]
        if self.__host == "":
            self.__host = "localhost"
        self.__port = int( parts[ 1 ] )
        self.__sock = None
        self.__readBuf = ""
        self.__replyDelimiter = re.compile( r'\r\n|\n|\r|\0' )
        return

    def connect( self, timeout ):
        " Connects to the server "
        self.__sock = socket.socket()
        self.__sock.settimeout( timeout )
        self.__sock.connect( (self.__host, self.__port) )
        return

    def disconnect( self ):
        " Close direct connection if it was opened "
        if self.__sock is not None:
            self.__sock.close()
        self.__sock = None
        return

    def login( self ):
        " Performs a direct login to NS "
        self.__sock.send( "netschedule_admin client_node=" + CLIENT_NODE +
                          " client_session=check_session client_type=admin\n\n" )
        return

    def execute( self, cmd, multiline = False):
        " Sends the given command to NS "
        self.__sock.sendall( cmd + "\n" )
        if multiline:
            return self.readMultiLineReply()
        return self.readSingleLineReply()

    def readSingleLineReply( self ):
        " Reads a single line reply "
        while True:
            parts = self.__replyDelimiter.split( self.__readBuf, 1 )
            if len( parts ) > 1:
                reply = parts[ 0 ]
                self.__readBuf = parts[ 1 ]
                break

            buf = self.__sock.recv( 8192 )
            if not buf:
                if self.__readBuf:
                    return self.__readBuf.strip()
                raise UnexpectedNSResponse( "Unexpected NS response: None" )
            self.__readBuf += buf

        if reply.startswith( "OK:" ):
            return reply.split( ':', 1 )[ 1 ].strip()
        if reply.startswith( "ERR:" ):
            if "shuttingdown" in reply.lower():
                raise NSShuttingDown( "Server is shutting down" )
            elif "access denied" in reply.lower():
                raise NSAccessDenied( reply.split( ':', 1 )[ 1 ].strip() )
            raise NSError( reply.split( ':', 1 )[ 1 ].strip() )
        raise UnexpectedNSResponse( "Unexpected NS response: " + reply.strip() )

    def readMultiLineReply( self ):
        " Reads a multi line reply "
        lines = []
        oneLine = self.readSingleLineReply()
        while oneLine != "END":
            if oneLine:
                lines.append( oneLine )
            oneLine = self.readSingleLineReply()
        return lines



LAST_EXIT_CODE = None
# Try to extract last check error code first
if len( sys.argv ) >= 4:
    try:
        LAST_EXIT_CODE = int( sys.argv[ 3 ] )
    except:
        pass


def adjustReturnCode( code ):
    " Adjusts the return code depending on the current script mode "

    if SCRIPT_MODE == ScriptMode.LBSMD222_OLD_CLIENT:
        printVerbose( "ScriptMode.LBSMD222_OLD_CLIENT is ON. "
                      "Adjusting return value. Was: " +
                      str( code ) )
        if code >= BASE_RESERVE_CODE and \
           code <= (BASE_RESERVE_CODE + 10):
            code += BASE_STANDBY_CODE - BASE_RESERVE_CODE

        printVerbose( "Adjusted to: " + str( code ) )

    # Otherwise no adjustments required
    return code


def log( code, message ):
    " Logs if it was not the last check return code "
    adjustedCode = adjustReturnCode( code )
    if str( adjustedCode ) != str( LAST_EXIT_CODE ):
        printStderr( message )
    return code


def main():
    " main function for the netschedule health check "

    lastCheckExitCode = None
    lastCheckRepeatCount = None
    secondsSinceLastCheck = None

    try:
        parser = OptionParser(
        """
        %prog  <service name> <connection point> [ lastCheckCode [ lastCheckRepeatCount [ timeSinceLastCheck ] ] ]
        """ )
        parser.add_option( "-v", "--verbose",
                           action="store_true", dest="verbose", default=False,
                           help="be verbose (default: False)" )

        # parse the command line options
        options, args = parser.parse_args()
        global VERBOSE
        VERBOSE = options.verbose

        if len( args ) < 2:
            return log( BASE_NO_ACTION_ALERT_CODE + 0,
                        "Incorrect number of arguments" )

        # These arguments are always available
        serviceName = args[ 0 ]                 # analysis:ignore
        connectionPoint = args[ 1 ]             # analysis:ignore
        if len( connectionPoint.split( ":" ) ) != 2:
            return log( BASE_NO_ACTION_ALERT_CODE + 1,
                        "invalid connection point format. Expected host:port" )

        # Arguments below may be missed
        if len( args ) > 2:
            lastCheckExitCode = int( args[ 2 ] )
        if len( args ) > 3:
            lastCheckRepeatCount = int( args[ 3 ] )
        if len( args ) > 4:
            secondsSinceLastCheck = int( args[ 4 ] )

        printVerbose( "Service name: " + serviceName )
        printVerbose( "Connection point: " + connectionPoint )
        printVerbose( "Last check exit code: " + str( lastCheckExitCode ) )
        printVerbose( "Last check repeat count: " + str( lastCheckRepeatCount ) )
        printVerbose( "Seconds since last check: " + str( secondsSinceLastCheck ) )
    except Exception, exc:
        return log( BASE_NO_ACTION_ALERT_CODE + 2,
                    "Error processing command line arguments: " + str( exc ) )


    # First stage: connection
    try:
        nsConnect = NSDirectConnect( connectionPoint )
        nsConnect.connect( COMMUNICATION_TIMEOUT )
        nsConnect.login()
    except socket.timeout, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 1,
                     "Error connecting to server: timeout" ) )
    except Exception, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 2,
                     "Error connecting to server: " + str( exc ) ) )
    except:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 3,
                     "Unknown check script error at the login stage" ) )


    canSetClientData = False
    switchingDone = False
    # Second stage: 
    try:
        # Check the server state. It could be in a drained shutdown
        if isInDrainedShutdown( nsConnect ):
            return BASE_RESERVE_CODE

        serverVersion = getServerVersion( nsConnect )
        if serverVersion >= StrictVersion( '4.17.0' ):
            # SETCLIENTDATA is available. Randomize the start and read client data
            sleep( float( randrange( 0, 1000 ) ) / 10000 )

            if testQueueExists( nsConnect ):
                # The queue must exist to read something from it
                printVerbose( "Switching to " + DYNAMIC_QUEUE_TO_TEST + " queue" )
                nsConnect.execute( "SETQUEUE " + DYNAMIC_QUEUE_TO_TEST )
                canSetClientData = True
                switchingDone = True

                # Read the verb, timestamp and the value
                verb, tm, value = getClientData( nsConnect )
                if verb is not None:
                    if verb == "DONE":
                        delta = datetime.datetime.now() - tm
                        if delta.total_seconds() < OTHER_CLIENT_DELAY:
                            printVerbose( "Using other client value immediately: " + str( value ) )
                            return value
                        printVerbose( "Other client results are obsolete" )
                    elif verb == "START":
                        delta = datetime.datetime.now() - tm
                        if delta.total_seconds() < OTHER_CLIENT_DELAY:
                            sleep( OTHER_CLIENT_DELAY )
                            verb, tm, value = getClientData( nsConnect )
                            if verb == "DONE":
                                printVerbose( "Using other client value after waiting: " + str( value ) )
                                return value
                        else:
                            printVerbose( "Other client started too long ago" )

        # Here: normal calculation of the penalty value
        if serverVersion >= StrictVersion( '4.16.10' ):
            staticChecker = StaticHealthChecker( nsConnect )
            staticChecker.check()   # Throws an exception

        if not testQueueExists( nsConnect ):
            if not testDefaultQueueClassExists( nsConnect ):
                return log( BASE_NO_ACTION_ALERT_CODE + 3,
                            "default queue class has not been found" )
            createTestQueue( nsConnect )

        if not switchingDone:
            printVerbose( "Switching to " + DYNAMIC_QUEUE_TO_TEST + " queue" )
            nsConnect.execute( "SETQUEUE " + DYNAMIC_QUEUE_TO_TEST )
        if not testQueueAcceptSubmit( nsConnect ):
            penaltyValue = log( BASE_NO_ACTION_ALERT_CODE + 4,
                                "test queue refuses submits" )
            if canSetClientData:
                setClientData( nsConnect, "DONE", penaltyValue )
            return penaltyValue

        start = datetime.datetime.now()
        setClientData( nsConnect, "START" )
        operationTest( nsConnect, serviceName )
        end = datetime.datetime.now()

    except socket.timeout, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 4,
                     "(service " + serviceName + ") communication timeout" ) )
    except UnexpectedNSResponse, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 5,
                     "(service " + serviceName + ") " + str( exc ) ) )
    except NSError, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 6,
                     "(service " + serviceName + ") " + str( exc ) ) )
    except NSShuttingDown, exc:
        return log( BASE_DOWN_CODE,
                    "(service " + serviceName + ") " + str( exc ) )
    except NSAccessDenied, exc:
        return log( BASE_NO_ACTION_ALERT_CODE + 5,
                    "(service " + serviceName + ") " + str( exc ) )
    except NSStaticCheckError, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 7,
                     "(service " + serviceName + ") " + str( exc ) ) )
    except Exception, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_NO_ACTION_ALERT_CODE + 6,
                     "(service " + serviceName + ") " + str( exc ) ) )
    except:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_NO_ACTION_ALERT_CODE + 6,
                     "(service " + serviceName + ") Unknown check script error" ) )

    # Everything is fine
    # Calculate the value basing on the measured
    delta = end - start
    deltaAsFloat = deltaToFloat( delta )
    printVerbose( "Delta: " + str( delta ) +
                  " as float: " + str( deltaAsFloat ) )

    penaltyValue = pickPenaltyValue( lastCheckExitCode,
                                     calcPenalty( deltaAsFloat ) )
    try:
        setClientData( nsConnect, "DONE", penaltyValue )
    except:
        pass
    return penaltyValue

def deltaToFloat( delta ):
    " Converts time delta to float "
    return delta.seconds + delta.microseconds / 1E6 + delta.days * 86400


def calcPenalty( execTime ):
    " Calculates the penalty basing on the execution time (sec, float) "
    execTime = int( execTime * 1000 )
    if execTime <= 20:
        return 0
    if execTime >= 500:
        return 99

    # Two intervals: 21 - 200   -> values from 0 - 70
    #                201 - 500  -> values from 70 - 99
    if execTime <= 200:
        k = 70.0 / (200.0 - 21.0)
        penalty = float((execTime - 20)) * k
    else:
        k = 29.0 / (500.0 - 201.0)
        penalty = float((execTime - 200)) * k + 70
    return int( penalty )


def pickPenaltyValue( lastCheckExitCode, calculatedCode ):
    " Provides a new value for the returned penalty "
    if lastCheckExitCode is None:
        lastCheckExitCode = BASE_RESERVE_CODE      # By default the previous
                                                    # return code is 100

    # No intersection => the calculated value
    # There is intersection => the point of intersection
    if lastCheckExitCode == calculatedCode:
        return calculatedCode

    if calculatedCode >= 100:
        return calculatedCode

    minRange = min( [lastCheckExitCode, calculatedCode] )
    maxRange = max( [lastCheckExitCode, calculatedCode] )
    crossedPoints = []
    for value in PENALTY_CURVE:
        if value > minRange and value < maxRange:
            crossedPoints.append( value )

    if len( crossedPoints ) == 0:
        return calculatedCode

    if calculatedCode < lastCheckExitCode:
        # Getting better
        return crossedPoints[ -1 ]
    # Getting worse
    return crossedPoints[ 0 ]


def getServerVersion( nsConnect ):
    " Retrieves the server version "
    printVerbose( "Getting Netschedule version" )
    output = nsConnect.execute( "VERSION" )
    values = cgi.parse_qs( output )
    return StrictVersion( values[ 'server_version' ][ 0 ] )


def getClientData( nsConnect ):
    """ Provides the client data:
        verb  - DONE/START or None
        tm    - timestamp or None
        value - last check result or None """
    printVerbose( "Getting client data" )
    output = nsConnect.execute( "STAT CLIENTS", True )
    pattern = "CLIENT: '" + CLIENT_NODE + "'"
    waiting = False
    for line in output:
        if line == pattern:
            waiting = True
            continue
        if waiting:
            line = line.strip()
            if line.startswith( "DATA: " ):
                line = line[ 5 : ].strip()
                data = line[ 1 : -1 ]
                if data:
                    parts = data.split( ' ' )
                    if len( parts ) not in [ 3, 4 ]:
                        return None, None
                    verb = parts[ 0 ]
                    tm = datetime.datetime.strptime(
                            parts[ 1 ] + " " + parts[ 2 ],
                            "%Y-%m-%d %H:%M:%S.%f" )
                    if len( parts ) == 3:
                        # No value, only a verb and a timestamp
                        # START 2014-10-06 14:51:49.242633
                        return verb, tm, None
                    # A verb, a timestamp and a value
                    # DONE 2014-10-06 14:51:49.242633 14
                    value = int( parts[ 3 ] )
                    return verb, tm, value
                return None, None, None
    return None, None, None

def setClientData( nsConnect, verb, value = None ):
    " Sets the client data unconditionally "
    data = verb + " " + datetime.datetime.now().strftime( "%Y-%m-%d %H:%M:%S.%f" )
    if value is not None:
        data += " " + str( value )
    printVerbose( "Setting client data: " + data )

    cmd = 'SETCLIENTDATA data="' + data + '" version=-1'
    nsConnect.execute( cmd )
    return

def isInDrainedShutdown( nsConnect ):
    " Checks the drain shutdown status "
    printVerbose( "Checking drained shutdown status" )
    output = nsConnect.execute( "STAT", True )
    for line in output:
        if 'drainedshutdown' in line.lower():
            parts = line.split( ":" )
            if len( parts ) != 2:
                raise UnexpectedNSResponse( "Unexpected NS output "
                                            "format for STAT" )
            return parts[ 1 ].strip() != "0"
    raise UnexpectedNSResponse( "Unexpected NS output format for STAT - "
                                "DrainedShutdow value is not found" )

def setCommunicationTimeout():
    " Sets the grid_cli communication timeout "
    printVerbose( "Setting communication timeout to " +
                  str( COMMUNICATION_TIMEOUT ) )
    varName = "NCBI_CONFIG__netservice_api__communication_timeout"
    os.environ[ varName ] = str( COMMUNICATION_TIMEOUT )
    return

def testQueueExists( nsConnect ):
    " True if the LBSMD test queue exists "
    printVerbose( "Testing if " + DYNAMIC_QUEUE_TO_TEST + " queue exists" )
    output = nsConnect.execute( "STAT QUEUES", True )
    pattern = "[queue " + DYNAMIC_QUEUE_TO_TEST + "]"
    for line in output:
        if line.startswith( pattern ):
            return True
    return False

def testQueueAcceptSubmit( nsConnect ):
    " True id the LBSMD test queue is not in the refuse submits state "
    printVerbose( "Testing if " + DYNAMIC_QUEUE_TO_TEST + " queue accepts submits" )
    output = nsConnect.execute( "QINF2 " + DYNAMIC_QUEUE_TO_TEST )
    values = cgi.parse_qs( output )
    if 'refuse_submits' in values:
        return values[ 'refuse_submits' ][ 0 ] == 'false'
    return True

def testDefaultQueueClassExists( nsConnect ):
    " True if 'default' queue class exists "
    printVerbose( "Testing if 'default' queue class exists" )
    output = nsConnect.execute( "STAT QCLASSES", True )
    pattern = "[qclass default]"
    for line in output:
        if line.startswith( pattern ):
            return True
    return False

def createTestQueue( nsConnect ):
    " Creates the test purpose dynamic queue "
    printVerbose( "Creating " + DYNAMIC_QUEUE_TO_TEST + " queue" )
    try:
        nsConnect.execute( "QCRE " + DYNAMIC_QUEUE_TO_TEST + " default" )
    except Exception, exc:
        # If many scripts try to create the same queue simultaneously
        # then an error 'already exists' is generated. Choke it if so.
        if 'already exists' in str( exc ).lower():
            return
        raise
    return

def operationTest( nsConnect, serviceName ):
    """ Tests the following operations:
        SUBMIT -> GET2 -> PUT2 -> SST -> WST """
    printVerbose( "Submitting a job to " + DYNAMIC_QUEUE_TO_TEST + " queue" )

    # affinity = str( os.getpid() )
    # Use service name as a test job affinity to avoid overflooding the
    # affinity registry when many instances of the script test the same NS
    # instance via different services
    affinity = "LBSMDTest_via_" + serviceName

    jobKey = nsConnect.execute( "SUBMIT NoInput aff=" + affinity )
    printVerbose( "Getting job status (WST2)" )
    output = nsConnect.execute( "WST2 " + jobKey )
    values = cgi.parse_qs( output )
    if values[ 'job_status' ][ 0 ] != "Pending":
        raise NSError( "Unexpected job status after submitting a job. "
                       "Expected: Pending Received: " +
                       values[ 'job_status' ][ 0 ] )
    printVerbose( "Getting a job" )
    output = nsConnect.execute( "GET2 wnode_aff=0 any_aff=0 "
                                "exclusive_new_aff=0 aff=" + affinity )
    values = cgi.parse_qs( output )
    if 'job_key' not in values:
        raise NSError( "Could not get submitted job" )
    jobKey = values[ 'job_key' ][ 0 ]
    authToken = values[ 'auth_token' ][ 0 ]
    printVerbose( "Getting job status (WST2)" )
    output = nsConnect.execute( "WST2 " + jobKey )
    values = cgi.parse_qs( output )
    if values[ 'job_status' ][ 0 ] != "Running":
        raise NSError( "Unexpected job status after getting it for "
                       "execution. Expected: Running Received: " +
                       values[ 'job_status' ][ 0 ] )
    printVerbose( "Putting job results" )
    nsConnect.execute( "PUT2 " + jobKey + " " + authToken + " 0 NoOutput" )
    printVerbose( "Getting job status (WST2)" )
    output = nsConnect.execute( "WST2 " + jobKey )
    values = cgi.parse_qs( output )
    if values[ 'job_status' ][ 0 ] != "Done":
        raise NSError( "Unexpected job status after submitting a job. "
                       "Expected: Done Received: " +
                       values[ 'job_status' ][ 0 ] )
    printVerbose( "Getting job status (SST2)" )
    output = nsConnect.execute( "SST2 " + jobKey )
    values = cgi.parse_qs( output )
    if values[ 'job_status' ][ 0 ] != "Done":
        raise NSError( "Unexpected job status after submitting a job. "
                       "Expected: Done Received: " +
                       values[ 'job_status' ][ 0 ] )
    return



class StaticHealthChecker:
    " Helper class to check FD and memory limits "
    def __init__( self, nsConnect ):
        self.__nsConnect = nsConnect
        return

    def check( self ):
        " Checks the limits "
        output = self.__nsConnect.execute( "HEALTH" )
        values = cgi.parse_qs( output )
        self.__checkMemory( values )
        self.__checkFD( values )
        return

    @staticmethod
    def __checkMemory( values ):
        " Checks the memory limit and throws an exception if exceeded "
        printVerbose( "Checking consumed memory" )
        try:
            physicalMemory = float( values[ "physical_memory" ][ 0 ] )
            usedMemory = float( values[ "mem_used_total" ][ 0 ] )
        except:
            raise UnexpectedNSResponse( "Unexpected output for HEALTH command" )
        percent = usedMemory / physicalMemory * 100.0
        if percent >= MEMORY_LIMIT:
            raise NSStaticCheckError( "Memory consumption " + str( percent ) +
                             "% exceeds the limit (" + str( MEMORY_LIMIT ) +
                             "%)" )
        return

    @staticmethod
    def __checkFD( values ):
        " Checks the FD limit and throws an exception if exceeded "
        printVerbose( "Checking consumed fd" )
        try:
            if values[ "proc_fd_soft_limit" ][ 0 ] == 'n/a':
                return
            if values[ "proc_fd_used" ][ 0 ] == 'n/a':
                return

            fdLimit = int( values[ "proc_fd_soft_limit" ][ 0 ] )
            fdUsed = int( values[ "proc_fd_used" ][ 0 ] )
        except:
            raise UnexpectedNSResponse( "Unexpected output for HEALTH command" )
        if fdLimit - fdUsed < FD_LIMIT:
            raise NSStaticCheckError( "FD consumption " + str( fdUsed ) +
                             " exceeds the limit (at least " +
                             str( FD_LIMIT ) + " must be available out of " +
                             str( fdLimit ) + " available for the process)" )
        return


# The script execution entry point
if __name__ == "__main__":
    printVerbose( "---------- Start ----------" )
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        printStderr( "Ctrl + C received" )
        returnValue = BASE_NO_ACTION_ALERT_CODE + 7
    except Exception, excpt:
        printStderr( str( excpt ) )
        returnValue = BASE_NO_ACTION_ALERT_CODE + 8
    except:
        printStderr( "Generic unknown script error" )
        returnValue = BASE_NO_ACTION_ALERT_CODE + 9


    printVerbose( "Return code: " + str( returnValue ) )
    returnValue = adjustReturnCode( returnValue )

    if str( LAST_EXIT_CODE ) == str( returnValue ):
        returnValue = NO_CHANGE_CODE

    sys.exit( returnValue )

