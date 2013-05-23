#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Interactive console to imitate NetStorage packets
"""

import sys
import socket
import errno
from optparse import OptionParser
from ncbi_grid_dev.ncbi import json_over_uttp, uttp
import pprint
import readline
#import rlcompleter
import atexit
import os
import simplejson as json

historyPath = os.path.expanduser( "~/.nstconsole" )

readline.parse_and_bind( 'tab: complete' )

def save_rlhistory():
    " Saves the commands history to use it in the future sessions "
    readline.write_history_file( historyPath )
    return

if os.path.exists( historyPath ):
    readline.read_history_file( historyPath )

atexit.register( save_rlhistory )


try:
    hostIP = socket.gethostbyname( socket.gethostname() )
except:
    hostIP = "127.0.0.1"



class NetStorageConsole:
    " NetStorage debug console implementation "

    def __init__( self, host_, port_ ):
        # Initialize the command map
        self.__commandMap = {
             '?':              self.printCommandList,
             'help':           self.printCommandList,
             'hello':          self.sendHello,
             'bye':            self.sendBye,
             'info':           self.sendInfo,
             'configuration':  self.sendConfiguration,
             'shutdown':       self.sendShutdown,
             'getclientsinfo': self.sendGetClientsInfo,
             'getobjectinfo':  self.sendGetObjectInfo,
             'getattr':        self.sendGetAttr,
             'setattr':        self.sendSetAttr,
             'no-type':        self.sendNoType,
             'no-dict':        self.sendNoDictionary,
             'upload':         self.upload,
             'delete':         self.delete,
             'download':       self.download,
             'exists':         self.exists,
             'getsize':        self.getsize,
           }

        self.__commandSN = 0

        # Create the connection
        socket.socket.write = socket.socket.send
        socket.socket.flush = lambda ignore: ignore
        socket.socket.readline = socket.socket.recv

        self.__sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
        self.__sock.connect( ( host_, port_ ) )

        self.__nst = json_over_uttp.MessageExchange( self.__sock, self.__sock )

    def clientMain( self ):
        " The driver "

        # Main loop
        userInput = ""
        while True:
            userInput = raw_input( "command >> " )
            userInput = userInput.strip()
            if userInput == "":
                self.checkIncomingMessage()
                continue

            # Split the input
            try:
                arguments = self.splitArguments( userInput )
            except Exception, exc:
                print "Error splitting command into parts: " + str( exc )
                continue

            # There is at least one argument which is a command name
            if arguments[ 0 ].lower() in [ 'exit', 'quit', 'q' ]:
                break

            # Find the command in the map
            processor = self.pickProcessor( arguments[ 0 ] )
            if processor is None:
                continue

            # Call the command
            processor( arguments[ 1 : ] )

        return 0


    def socketHasData( self ):
        " Checks if the socket has some data in it "
        try:
            data = self.__sock.recv( 8, socket.MSG_PEEK | socket.MSG_DONTWAIT )
            if len( data ) == 0:
                return False
        except socket.error, e:
            if e.args[0] == errno.EAGAIN:
                return False
            if e.args[0] == errno.ECONNRESET:
                print "Server socket has been closed"
                sys.exit( errno.ECONNRESET )
            raise
        return True

    def checkIncomingMessage( self ):
        " Checks if there is something in the socket and if so reads it "
        if self.socketHasData():
            response = self.__nst.receive()
            self.printMessage( "Message from server", response )
        return


    def pickProcessor( self, command ):
        " Picks the command from the map "
        if command in self.__commandMap:
            return self.__commandMap[ command.lower() ]

        # Try to find candidates
        candidates = []
        for key in self.__commandMap.keys():
            if key.startswith( command.lower() ):
                candidates.append( [ key, self.__commandMap[ key ] ] )

        if len( candidates ) == 0:
            print "The command '" + command + "' is not supported. " + \
                  "Type'help' to get the list of supported commands"
            return None

        if len( candidates ) == 1:
            return candidates[ 0 ][ 1 ]

        # ambiguity
        names = candidates[ 0 ][ 0 ]
        for cand in candidates[  1 : ]:
            names += ", " + cand[ 0 ]
        print "The command '" + command + \
              "' is ambiguous. Candidates are: " + names
        return None



    @staticmethod
    def splitArguments( cmdLine ):
        " Splits the given line into the parts "

        result = []

        cmdLine = cmdLine.strip()
        expectQuote = False
        expectDblQuote = False
        lastIndex = len( cmdLine ) - 1
        argument = ""
        index = 0
        while index <= lastIndex:
            if expectQuote:
                if cmdLine[ index ] == "'":
                    if cmdLine[ index - 1 ] != '\\':
                        if argument != "":
                            result.append( argument )
                            argument = ""
                        expectQuote = False
                    else:
                        argument = argument[ : -1 ] + "'"
                else:
                    argument += cmdLine[ index ]
                index += 1
                continue
            if expectDblQuote:
                if cmdLine[ index ] == '"':
                    if cmdLine[ index - 1 ] != '\\':
                        if argument != "":
                            result.append( argument )
                            argument = ""
                        expectDblQuote = False
                    else:
                        argument = argument[ : -1 ] + '"'
                else:
                    argument += cmdLine[ index ]
                index += 1
                continue
            # Not in a string literal
            if cmdLine[ index ] == "'":
                if index == 0 or cmdLine[ index - 1 ] != '\\':
                    expectQuote = True
                    if argument != "":
                        result.append( argument )
                        argument = ""
                else:
                    argument = argument[ : -1 ] + "'"
                index += 1
                continue
            if cmdLine[ index ] == '"':
                if index == 0 or cmdLine[ index - 1 ] != '\\':
                    expectDblQuote = True
                    if argument != "":
                        result.append( argument )
                        argument = ""
                else:
                    argument = argument[ : -1 ] + '"'
                index += 1
                continue
            if cmdLine[ index ] in [ ' ', '\t' ]:
                if argument != "":
                    result.append( argument )
                    argument = ""
                index += 1
                continue
            argument += cmdLine[ index ]
            index += 1


        if argument != "":
            result.append( argument )

        if expectQuote or expectDblQuote:
            raise Exception( "No closing quotation" )
        return result


    def printCommandList( self, arguments ):
        " Prints the available commands "
        print "Available commands:"
        for key in self.__commandMap.keys():
            print key
        return

    def sendHello( self, arguments ):
        " Sends the HELLO message "
        if len( arguments ) > 1:
            print "The 'hello' command accepts 0 or 1 argument (client name)"
            return

        client = "nstconsole - debugging NetStorage console"
        if len( arguments ) == 1:
            client = arguments[ 0 ]

        message = { 'Type':         'HELLO',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'Client':       client,
                    'Application':  'test/nstconsole.py',
                    'Ticket':       'No ticket at all' }
        self.exchange( message )
        return

    def sendBye( self, arguments ):
        " Sends the BYE message "
        if len( arguments ) != 0:
            print "The 'bye' command does not accept any arguments"
            return

        message = { 'Type':         'BYE',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendInfo( self, arguments ):
        " Sends INFO request "
        if len( arguments ) != 0:
            print "The 'info' command does not accept any arguments"
            return

        message = { 'Type':         'INFO',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendConfiguration( self, arguments ):
        " Sends CONFIGURATION request "
        if len( arguments ) != 0:
            print "The 'configuration' command does not accept any arguments"
            return

        message = { 'Type':         'CONFIGURATION',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendShutdown( self, arguments ):
        " Sends SHUTDOWN request "
        if len( arguments ) > 1:
            print "The 'shutdown' commands takes 0 " \
                  "(default: soft) or 1 argument "
            return

        mode = "soft"
        if len( arguments ) == 1:
            mode = arguments[ 0 ].lower()
            if mode != "soft" and mode != "hard":
                print "The allowed values of the argument are 'soft' and 'hard'"
                return

        message = { 'Type':         'SHUTDOWN',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'Mode':         mode }
        self.exchange( message )
        return

    def sendGetClientsInfo( self, arguments ):
        " Sends GETCLIENTSINFO request "
        if len( arguments ) != 0:
            print "The 'getclientsinfo' command does not accept any arguments"
            return

        message = { 'Type':         'GETCLIENTSINFO',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendNoType( self, arguments ):
        " Sends a malformed request without a type "
        message = { 'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendNoDictionary( self, arguments ):
        " Sends a malformed non-dictionary request "
        message = [ 'blah', 'blah', 'blah' ]
        self.printMessage( "Message to server", message )
        response = self.__nst.exchange( message )
        self.printMessage( "Message from server", response )
        return

    def upload( self, arguments ):
        " Uploads a file "
        if len( arguments ) < 1:
            print "At least one argument is required "
            return

        fileName = arguments[ 0 ]

        if not os.path.exists( fileName ):
            print "File '" + fileName + "' is not found"
            return

        try:
            f = open( fileName, "r" )
            content = f.read()
        except Exception, ex:
            print "File operation error: " + str( ex )
            return

        message = { 'Type':         'WRITE',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP }
        if len( arguments ) > 1:
            message.update( json.loads( ' '.join( arguments[ 1 : ] ) ) )

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed, no data will be transferred"
            return

        uttp_writer = self.__nst.get_uttp_writer()
        chunk = uttp_writer.send_chunk(content)
        if chunk:
            self.__sock.send(chunk)
        chunk = uttp_writer.flush_buf()
        if chunk:
            self.__sock.send(chunk)

        response = self.receive()
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed, the blob has not been written [completely]"
        return

    def delete( self, arguments ):
        " Deletes the given object "

        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileID = arguments[ 0 ]

        message = { 'Type':         'DELETE',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'FileID':       fileID }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def download( self, arguments ):
        " Reads the given object "

        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileID = arguments[ 0 ]

        message = { 'Type':         'READ',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'FileID':       fileID }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
            return

        uttp_reader = self.__nst.get_uttp_reader()

        while True:
            buf = self.__sock.recv( 1024 * 1024 )
            uttp_reader.set_new_buf(buf)
            while True:
                event = uttp_reader.next_event()
                if event == uttp.Reader.END_OF_BUFFER:
                    break

                if event == uttp.Reader.CHUNK_PART:
                    print uttp_reader.get_chunk(),
                elif event == uttp.Reader.CHUNK:
                    print uttp_reader.get_chunk() + '<<EOF'
                    response = self.receive()
                    if "Status" not in response or response[ "Status" ] != "OK":
                        print "Command failed"
                    return
                else:
                    raise Exception( "Unexpected UTTP packet type" )

    def exists( self, arguments ):
        " Tells if an object exists "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileID = arguments[ 0 ]
        message = { 'Type':         'EXISTS',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'FileID':       fileID }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def getsize( self, arguments ):
        " Tells if an object exists "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileID = arguments[ 0 ]
        message = { 'Type':         'GETSIZE',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'FileID':       fileID }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendGetObjectInfo( self, arguments ):
        " Sends the getobjectinfo request "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileID = arguments[ 0 ]
        message = { 'Type':         'GETOBJECTINFO',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'FileID':       fileID }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendGetAttr( self, arguments ):
        " Sends GETATTR message "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileID = arguments[ 0 ]

        message = { 'Type':         'GETATTR',
                    'SessionID':    '1111111111111111_0000SID',
                    'ClientIP':     hostIP,
                    'FileID':       fileID }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendSetAttr( self, arguments ):
        " Sends SETATTR message "
        print "Not implemented yet"
        return


    def exchange( self, message ):
        " Does the basic exchange "
        self.__commandSN += 1

        message[ "SN" ] = self.__commandSN
        self.printMessage( "Message to server", message )
        response = self.__nst.exchange( message )
        self.printMessage( "Message from server", response )
        return response


    def receive( self ):
        " Receives a single server message "
        response = self.__nst.receive()
        self.printMessage( "Message from server", response )
        return response


    @staticmethod
    def printMessage( prefix, response ):
        " Prints the server response "
        print prefix + ":"
        prettyPrinter = pprint.PrettyPrinter( indent = 4 )
        prettyPrinter.pprint( response )
        return


def parserError( parser_, message ):
    " Prints the message and help on stderr "
    sys.stdout = sys.stderr
    print message
    parser_.print_help()
    return

# The script execution entry point
if __name__ == "__main__":
    try:
        parser = OptionParser(
        """
        %prog  <host>  <port>
        Netstorage debug console
        """ )

        # parser.add_option( "-v", "--verbose",
        #                   action="store_true", dest="verbose", default=False,
        #                   help="be verbose (default: False)" )

        # parse the command line options
        options, args = parser.parse_args()

        if len( args ) != 2:
            parserError( parser, "Incorrect number of arguments" )
            sys.exit( 1 )

        host = args[ 0 ]
        port = args[ 1 ]
        try:
            port = int( port )
        except:
            parserError( parser, "The port must be integer" )
            sys.exit( 1 )

        returnValue = NetStorageConsole(host, port).clientMain()
    except Exception, exc:
        print >> sys.stderr, str( exc )
        returnValue = 4

    sys.exit( returnValue )
