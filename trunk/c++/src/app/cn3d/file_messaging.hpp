/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Paul Thiessen
*
* File Description:
*       file-based messaging system
*
* ===========================================================================
*/

#ifndef FILE_MESSAGING__HPP
#define FILE_MESSAGING__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbifile.hpp>

#include <string>
#include <map>
#include <list>


BEGIN_NCBI_SCOPE

//
// FileMessageResponder is a mix-in class - an interface-only class that defines callback methods that
// will be used to pass messages to an application object of this type. For both commands and replies,
// if the 'data' string is not empty (size()>0), then it will be a series of '\n'-terminated lines.
//
class MessageResponder
{
public:
    // called when a (new) command is received
    virtual void ReceivedCommand(const std::string& fromApp, unsigned long id,
        const std::string& command, const std::string& data) = 0;

    // possible reply status values
    enum ReplyStatus {
        REPLY_OKAY,
        REPLY_ERROR
    };

    // called when a reply is received
    virtual void ReceivedReply(const std::string& fromApp, unsigned long id,
        ReplyStatus status, const std::string& data) = 0;
};

//
// FileMessenger represents an individual message file connection
//
class FileMessagingManager;

class FileMessenger
{
private:
    // (de)construction handled only by FileMessagingManager parent
    friend class FileMessagingManager;
    FileMessenger(FileMessagingManager *parentManager,
        const std::string& messageFilename, MessageResponder *responderObject, bool isReadOnly);
    ~FileMessenger(void);

    const FileMessagingManager * const manager;
    const CDirEntry messageFile, lockFile;
    MessageResponder * const responder;
    const bool readOnly;

    // keep track of id's used and sent to/from which recipient, and whether reply was received
    typedef std::map < std::string , std::string > TargetApp2Command;
    typedef std::map < unsigned long , TargetApp2Command > CommandOriginators;
    CommandOriginators commandsSent, commandsReceived;

    typedef std::map < std::string , MessageResponder::ReplyStatus > TargetApp2Status;
    typedef std::map < unsigned long , TargetApp2Status > CommandReplies;
    CommandReplies repliesReceived, repliesSent;

    // keep a queue of commands to send next time the file is opened for writing
    typedef struct {
        std::string to;
        unsigned long id;
        std::string command, data;
    } CommandInfo;
    typedef std::list < CommandInfo > CommandList;
    CommandList pendingCommands;

    // keep track of file size last time the file was read
    Int8 lastKnownSize;

    // read the file and process any new commands
    void ReceiveCommands(void);

    // send any commands waiting in the queue
    void SendPendingCommands(void);

public:
    // send a new command (must have a unique id for given target app)
    void SendCommand(const std::string& targetApp, unsigned long id,
        const std::string& command, const std::string& data);

    // send reply to previously received command
    void SendReply(const std::string& targetApp, unsigned long id,
        MessageResponder::ReplyStatus status, const std::string& data);

    // receives any new commands/replies and sends any pending ones
    void PollMessageFile(void);
};

//
// FileMessagingManager is a container for FileMessengers, mostly as a convenience
// to poll all message files from one central function
//
class FileMessagingManager
{
private:
    friend class FileMessenger;
    const std::string applicationName;

    typedef std::list < FileMessenger * > FileMessengerList;
    FileMessengerList messengers;

public:
    FileMessagingManager(const std::string& appName);
    // stops all monitoring and destroys all messenger objects, but does not delete any message files
    ~FileMessagingManager(void);

    // to create a new message file connection; if readOnly==false then replies are sent through
    // the same file, otherwise replies are logged internally but the message file isn't changed
    FileMessenger * CreateNewFileMessenger(const std::string& messageFilename,
        MessageResponder *responderObject, bool readOnly);

    // stop monitoring a connection (and destroy the messenger), and optionally delete the message file.
    void DeleteFileMessenger(FileMessenger *messenger, bool deleteMessageFile);

    // for all FileMessengers, receives any new commands/replies and sends any pending ones
    void PollMessageFiles(void);
};

END_NCBI_SCOPE

#endif // FILE_MESSAGING__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/03/19 14:44:36  thiessen
* fix char/traits problem
*
* Revision 1.2  2003/03/13 18:55:04  thiessen
* add messenger destroy function
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/
