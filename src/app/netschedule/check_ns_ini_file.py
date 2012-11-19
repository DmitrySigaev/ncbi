#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Utility to compare NS config files
"""

import sys
import os
import tempfile
from sets import Set
from copy import deepcopy
from StringIO import StringIO
from optparse import OptionParser
from subprocess import Popen, PIPE
from ConfigParser import ConfigParser


SVN_BASE_PATH = "https://svn.ncbi.nlm.nih.gov/repos/" \
                "toolkit/release/netschedule/"
SVN_RELEASE_PATH = "/c++/src/app/netschedule/netscheduled.ini"


def main():
    " The utility entry point "

    parser = OptionParser(
    """
    %prog  <local NS ini file>  <pattern NS version> [options]

    e.g. %prog  netscheduled.ini  4.16.0
    NetSchedule configuration file checking utility.
    It compares the given file and the pattern .ini file for the given release
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )

    # parse the command line options
    options, args = parser.parse_args()
    verbose = options.verbose

    # Check the number of arguments
    if len( args ) != 2:
        return parserError( parser, "Incorrect number of arguments" )

    localIniFile = args[ 0 ]
    patternNSVersion = args[ 1 ]

    if not os.path.exists( localIniFile ):
        print >> sys.stderr, "Cannot find local NS ini file " + localIniFile
        return 1
    if not os.path.isfile( localIniFile ):
        print >> sys.stderr, "Local NS ini file must be a file name. " \
                             "The path " + localIniFile + " is not a file."
        return 1

    svnPath = SVN_BASE_PATH + patternNSVersion + SVN_RELEASE_PATH
    if not doesURLExist( svnPath ):
        print >> sys.stderr, "The pattern netschedule.ini file " \
                             "is not found: " + svnPath
        return 1

    if verbose:
        print "Local config file: " + localIniFile
        print "Pattern config file: " + svnPath


    # Get the content of the pattern file
    try:
        content = safeRun( [ 'svn', 'cat', svnPath ] )
    except:
        print >> sys.stderr, "Cannot get content of the pattern " \
                             "config file from SVN: " + svnPath
        return 1

    # Compose parsed configs
    localConfig = ConfigParser()
    localConfig.readfp( open( localIniFile ) )

    patternConfig = ConfigParser()
    patternConfig.readfp( StringIO( content ) )


    localSections = localConfig.sections()
    patternSections = patternConfig.sections()

    # Sort the found sections
    lClasses, lQueues, lOther = splitSections( localSections )
    pClasses, pQueues, pOther = splitSections( patternSections )

    # pClasses has no need
    pClasses = pClasses
    if len( pQueues ) != 1:
        raise Exception( "Unexpected number of queues in the pattern file" )

    # Compare other sections
    retCode  = compareOtherSections( localConfig, patternConfig,
                                     lOther, pOther )

    allowedQueueValues = Set( tuplesToValues( patternConfig.items( pQueues[ 0 ] ) ) )
    allowedClassValues = deepcopy( allowedQueueValues )
    if "class" in allowedClassValues:
        allowedClassValues.remove( "class" )
    allowedClassValues = Set( allowedClassValues )

    for qclass in lClasses:
        retCode += compareQueueClass( localConfig, qclass, allowedClassValues )

    for queue in lQueues:
        retCode += compareQueue( localConfig, queue, allowedQueueValues )

    return retCode


def compareQueue( localConfig, queue, allowedQueueValues ):
    " Compares a single queue "
    retCode = 0
    localValues = Set( tuplesToValues( localConfig.items( queue ) ) )

    missed = allowedQueueValues - localValues
    extra = localValues - allowedQueueValues

    if len( missed ) >= 1:
        print >> sys.stderr, "Local config file misses the following values " \
                             "in the queue [" + queue + "] description:"
        print >> sys.stderr, "\n".join( missed )
        retCode += 1

    if len( extra ) >= 1:
        print >> sys.stderr, "Local config file has the following extra " \
                             "values in the queue [" + queue + \
                             "] description:"
        print >> sys.stderr, "\n".join( extra )
        retCode += 1

    return retCode


def compareQueueClass( localConfig, qclass, allowedClassValues ):
    " Compares a single queue class "
    retCode = 0
    localValues = Set( tuplesToValues( localConfig.items( qclass ) ) )

    missed = allowedClassValues - localValues
    extra = localValues - allowedClassValues

    if len( missed ) >= 1:
        print >> sys.stderr, "Local config file misses the following values " \
                             "in the queue class [" + qclass + "] description:"
        print >> sys.stderr, "\n".join( missed )
        retCode += 1

    if len( extra ) >= 1:
        print >> sys.stderr, "Local config file has the following extra " \
                             "values in the queue class [" + qclass + \
                             "] description:"
        print >> sys.stderr, "\n".join( extra )
        retCode += 1

    return retCode

def compareOtherSections( localConfig, patternConfig, lOther, pOther ):
    " Compares and prints the difference of the other sections "

    retCode = 0
    localSet = Set( lOther )
    patternSet = Set( pOther )

    missed = patternSet - localSet
    extra = localSet - patternSet
    common = patternSet & localSet

    if len( missed ) >= 1:
        print >> sys.stderr, "Local config file misses the following sections:"
        print >> sys.stderr, "\n".join( missed )
        retCode += 1

    if len( extra ) >= 1:
        print >> sys.stderr, "Local config file has " \
                             "the following extra sections:"
        print >> sys.stderr, "\n".join( extra )
        retCode += 1

    for item in common:
        retCode += compareSectionItems( localConfig, patternConfig, item )

    return retCode


def compareSectionItems( localConfig, patternConfig, section ):
    " Compares the section items "

    retCode = 0
    localValues = Set( tuplesToValues( localConfig.items( section ) ) )
    patternValues = Set( tuplesToValues( patternConfig.items( section ) ) )

    missed = patternValues - localValues
    extra = localValues - patternValues

    if len( missed ) >= 1:
        print >> sys.stderr, "Local config file section '" + section + \
                             "' misses the following values:"
        print >> sys.stderr, "\n".join( missed )
        retCode += 1

    if len( extra ) >= 1:
        print >> sys.stderr, "Local config file section '" + section + \
                             "' has the following extra values:"
        print >> sys.stderr, "\n".join( extra )
        retCode += 1

    return retCode


def splitSections( sections ):
    " Splits all the sections from an .ini file into 3 parts "

    qclasses = []
    queues = []
    other = []

    for item in sections:
        if item.startswith( "qclass_" ):
            qclasses.append( item )
        elif item.startswith( "queue_" ):
            queues.append( item )
        else:
            other.append( item )
    return qclasses, queues, other


def parserError( parser, message ):
    " Prints the message and help on stderr "
    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return 1

def doesURLExist( urlToTest ):
    " Checks if the given URL exists "
    try:
        safeRun( [ 'svn', 'info', urlToTest ] )
    except:
        return False
    return True

def safeRun( commandArgs ):
    " Provides the process stdout "
    stdOut, stdErr = safeRunWithStderr( commandArgs )
    stdErr = stdErr
    return stdOut


def safeRunWithStderr( commandArgs ):
    " Runs the given command and provides both stdout and stderr "

    errTmp = tempfile.mkstemp()
    errStream = os.fdopen( errTmp[ 0 ] )
    process = Popen( commandArgs, stdin = PIPE,
                     stdout = PIPE, stderr = errStream )
    process.stdin.close()
    processStdout = process.stdout.read()
    process.stdout.close()
    errStream.seek( 0 )
    err = errStream.read()
    errStream.close()
    process.wait()
    try:
        # On WinXX the file might still be kept and unlink generates exception
        os.unlink( errTmp[ 1 ] )
    except:
        pass

    # 'grep' return codes:
    # 0 - OK, lines found
    # 1 - OK, no lines found
    # 2 - Error occured

    if process.returncode == 0 or \
       ( os.path.basename( commandArgs[ 0 ] ) == "grep" and \
         process.returncode == 1 ):
        # No problems, the ret code is 0 or the grep special case
        return processStdout, err.strip()

    # A problem has been identified
    raise Exception( "Error in '%s' invocation: %s" % \
                     (commandArgs[0], err) )


def tuplesToValues( src ):
    " Converts a list of tuples to a list of first values from tuples "
    res = []
    for item in src:
        res.append( item[ 0 ] )
    return res


# The script execution entry point
if __name__ == "__main__":
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        print >> sys.stderr, "Ctrl + C received"
        returnValue = 2

    except Exception, excpt:
        print >> sys.stderr, str( excpt )
        returnValue = 1

    sys.exit( returnValue )
