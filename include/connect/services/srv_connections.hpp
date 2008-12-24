#ifndef CONNECT_SERVICES__SERVER_CONN_HPP_1
#define CONNECT_SERVICES__SERVER_CONN_HPP_1

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
 * Authors:  Maxim Didneko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include "netobject.hpp"

#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////
//
struct SNetServerCmdOutputImpl;

class NCBI_XCONNECT_EXPORT CNetServerCmdOutput
{
    NET_COMPONENT(NetServerCmdOutput);

    bool ReadLine(std::string& output);
};

///////////////////////////////////////////////////////////////////////////
//
struct SNetServerConnectionImpl;

class NCBI_XCONNECT_EXPORT CNetServerConnection
{
    NET_COMPONENT(NetServerConnection);

    // Execute remote command 'cmd', check if the reply
    // starts with 'OK:', and return the remaining
    // characters of the reply as a string.
    std::string Exec(const std::string& cmd);

    // Execute remote command 'cmd' and return a smart
    // pointer to a stream object for reading multiline
    // output of the command.
    CNetServerCmdOutput ExecMultiline(const std::string& cmd);

    class IStringProcessor {
    public:
        // if returns false the telnet method will stop
        virtual bool Process(string& line) = 0;
        virtual ~IStringProcessor() {}
    };
     // out and processor can be NULL
    void Telnet(CNcbiOstream* out,  IStringProcessor* processor);

    const string& GetHost() const;
    unsigned int GetPort() const;
};


///////////////////////////////////////////////////////////////////////////
//
class INetServerConnectionListener;

struct SNetServerConnectionPoolImpl;

class NCBI_XCONNECT_EXPORT CNetServerConnectionPool
{
    NET_COMPONENT(NetServerConnectionPool);

    const string& GetHost() const;
    unsigned short GetPort() const;

    void SetEventListener(INetServerConnectionListener* listener);
    CNetObjectRef<INetServerConnectionListener> GetEventListener() const;

    static void SetDefaultCommunicationTimeout(const STimeout& to);
    void SetCommunicationTimeout(const STimeout& to);
    const STimeout& GetCommunicationTimeout() const;

    static void SetDefaultCreateSocketMaxReties(unsigned int retires);
    void SetCreateSocketMaxRetries(unsigned int retries);
    unsigned int GetCreateSocketMaxRetries() const;

    void PermanentConnection(ESwitch type);

    CNetServerConnection GetConnection();
};


///////////////////////////////////////////////////////////////////////////
//
class INetServerConnectionListener : public CNetObject
{
public:
    virtual void OnConnected(const CNetServerConnection&) = 0;
    virtual void OnError(string& err_msg) = 0;
};


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SERVER_CONN_HPP
