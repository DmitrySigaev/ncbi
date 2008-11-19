#ifndef CONNECT_SERVICES__REMOTE_APP_MB_HPP
#define CONNECT_SERVICES__REMOTE_APP_MB_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:
 *
 */

#include <connect/services/remote_app.hpp>

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class IBlobStorageFactory;
class CRemoteAppRequestMB_Impl;

enum EStdOutErrStorageType {
    eLocalFile = 0,
    eBlobStorage
};

/// Remote Application Request (client side)
///
/// It is used by a client application which wants to run a remote application
/// through NetSchedule infrastructure and should be used in conjunction with
/// CGridJobSubmitter class
///
class NCBI_XCONNECT_EXPORT CRemoteAppRequestMB :
    public CObject,
    public IRemoteAppRequest
{
public:
    explicit CRemoteAppRequestMB(IBlobStorageFactory& factory);
    ~CRemoteAppRequestMB();

    /// Get an output stream to write data to a remote application stdin
    CNcbiOstream& GetStdIn();

    /// Set a remote application command line.
    /// Cmdline should not contain a remote program name, just its arguments
    void SetCmdLine(const string& cmd);

    void SetAppRunTimeout(unsigned int sec);

    /// Transfer a file to an application executer side.
    /// It only makes sense to transfer a file if its name also mentioned in
    /// the command line for the remote application. When the file is transfered
    /// the the executer side it gets stored to a temprary directory and then its
    /// original name in the command line will be replaced with the new temprary name.
    void AddFileForTransfer(const string& fname,
                            ETrasferType tt = eBlobStorage);

    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType storage_type = eLocalFile);

    void SetMaxInputSize(size_t max_input_size);


    /// Serialize a request to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    void Send(CNcbiOstream& os);

private:
    CRemoteAppRequestMB(const CRemoteAppRequestMB&);
    CRemoteAppRequestMB& operator=(const CRemoteAppRequestMB&);

    auto_ptr<CRemoteAppRequestMB_Impl> m_Impl;
};

/// Remote Application Request (application executer side)
///
/// It is used by a grid worker node to get parameters for a remote application.
///
class NCBI_XCONNECT_EXPORT CRemoteAppRequestMB_Executer : public IRemoteAppRequest_Executer
{
public:
    explicit CRemoteAppRequestMB_Executer(IBlobStorageFactory& factory);
    ~CRemoteAppRequestMB_Executer();

    /// Get a stdin stream for a remote application
    CNcbiIstream& GetStdIn();

    /// Get a commnad line for a remote application
    const string& GetCmdLine() const;

    unsigned int GetAppRunTimeout() const;
    //    bool IsExclusiveModeRequested() const;
    const string& GetWorkingDir() const;
    const string& GetInBlobIdOrData() const;

    /// Deserialize a request from a given stream.
    void Receive(CNcbiIstream& is);

    const string& GetStdOutFileName() const;
    const string& GetStdErrFileName() const;
    EStdOutErrStorageType GetStdOutErrStorageType() const;

    void Reset();

    void Log(const string& prefix);

private:
    CRemoteAppRequestMB_Executer(const CRemoteAppRequestMB_Executer &);
    CRemoteAppRequestMB_Executer& operator=(const CRemoteAppRequestMB_Executer&);

    auto_ptr<CRemoteAppRequestMB_Impl> m_Impl;
};


class CRemoteAppResultMB_Impl;

/// Remote Application Result (client side)
///
/// It is used by a client application to get results for a remote applicaion
/// and should be used in conjunction with CGridJobStatus
///
class NCBI_XCONNECT_EXPORT CRemoteAppResultMB :
    public CObject,
    public IRemoteAppResult
{
public:
    explicit CRemoteAppResultMB(IBlobStorageFactory& factory);
    ~CRemoteAppResultMB();

    /// Get a remote application stdout
    CNcbiIstream& GetStdOut();

    /// Get a remote application stderr
    CNcbiIstream& GetStdErr();

    /// Get a remote application return code
    int           GetRetCode() const;

    const string& GetStdOutFileName() const;
    const string& GetStdErrFileName() const;
    EStdOutErrStorageType GetStdOutErrStorageType() const;

    /// Deserialize a result from a given stream.
    void Receive(CNcbiIstream& is);

private:
    CRemoteAppResultMB(const CRemoteAppResultMB&);
    CRemoteAppResultMB& operator=(const CRemoteAppResultMB&);

    auto_ptr<CRemoteAppResultMB_Impl> m_Impl;
};

/// Remote Application Result (application executer side)
///
/// It is used by a grid worker node to send results of a
/// finished remote application to the clien.
class NCBI_XCONNECT_EXPORT CRemoteAppResultMB_Executer : public IRemoteAppResult_Executer
{
public:
    explicit CRemoteAppResultMB_Executer(IBlobStorageFactory& factory);
    ~CRemoteAppResultMB_Executer();

    /// Get a stream to put remote application's stdout to
    CNcbiOstream& GetStdOut();

    /// Get a stream to put remote application's stderr to
    CNcbiOstream& GetStdErr();

    /// Set a remote application's return code
    void SetRetCode(int ret_code);

    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType type);

    void SetMaxOutputSize(size_t max_output_size);


    /// Serialize a result to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    void Send(CNcbiOstream& os);

    const string& GetOutBlobIdOrData() const;
    const string& GetErrBlobIdOrData() const;

    void Reset();

    void Log(const string& prefix);

private:
    CRemoteAppResultMB_Executer(const CRemoteAppResultMB_Executer&);
    CRemoteAppResultMB_Executer& operator=(const CRemoteAppResultMB_Executer&);

    auto_ptr<CRemoteAppResultMB_Impl> m_Impl;
};


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__REMOTE_APP_MB_HPP
