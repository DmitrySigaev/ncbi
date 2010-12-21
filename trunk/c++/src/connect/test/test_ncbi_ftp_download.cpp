/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Special FTP download test for demonstration of streaming
 *   and callback capabilities of various Toolkit classes.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_util.h>
#include <util/compress/stream_util.hpp>
#include <util/compress/tar.hpp>
#include <string.h>
#if   defined(NCBI_OS_UNIX)
#  include <signal.h>
#  include <unistd.h>  //  isatty()
#elif defined(NCBI_OS_MSWIN)
#  include <io.h>      // _isatty()
#endif // NCBI_OS


BEGIN_NCBI_SCOPE


static const char kDefaultTestURL[] =
    "ftp://ftp:-none@ftp.ncbi.nlm.nih.gov/toolbox/ncbi_tools/ncbi.tar.gz";


static bool s_Signaled  = false;
static int  s_Throttler = 0;


#ifdef NCBI_OS_UNIX
extern "C" {
static void s_Interrupt(int /*signo*/)
{
    s_Signaled = true;
}
}
#endif // NCBI_OS_UNIX


class CDownloadCallbackData {
public:
    CDownloadCallbackData(const char* filename, Uint8 size = 0)
        : m_Sw(CStopWatch::eStart), m_Size(size), m_Last(0.0),
          m_Filename(filename)
    {
        memset(&m_Cb, 0, sizeof(m_Cb));
        m_Current = pair<string,Uint8>(kEmptyStr, 0);
    }

    SCONN_Callback *       CB(void)        { return &m_Cb;          }
    Uint8             GetSize(void) const  { return m_Size;         }
    void              SetSize(Uint8 size)  { m_Size = size;         }
    double         GetElapsed(bool update = true);
    const string& GetFilename(void) const  { return m_Filename;     }

    void                  Append(const CTar::TFile* current = 0);
    const CTar::TFiles& Filelist(void) const { return m_Filelist; }

private:
    CStopWatch     m_Sw;
    SCONN_Callback m_Cb;
    Uint8          m_Size;
    double         m_Last;
    CTar::TFile    m_Current;
    const string   m_Filename;
    CTar::TFiles   m_Filelist;
};


double CDownloadCallbackData::GetElapsed(bool update)
{
    double next = m_Sw.Elapsed();
    if (!update  &&  m_Last + 0.5 > next) {
        return 0.0;
    }
    m_Last = next;
    return next;
}


void CDownloadCallbackData::Append(const CTar::TFile* current)
{
    if (m_Current.first.empty()) {
        // first time
        _ASSERT(m_Filelist.empty());
    } else {
        m_Filelist.push_back(m_Current);
    }
    m_Current = current ? *current : pair<string,Uint8>(kEmptyStr, 0);
}


class CTarCheckpointed : public CTar {
public:
    CTarCheckpointed(istream& is, CDownloadCallbackData* dlcbdata)
        : CTar(is), m_Dlcbdata(dlcbdata)
    { }

protected:
    virtual bool Checkpoint(const CTarEntryInfo& current, bool /**/) const
    {
        if (!m_Dlcbdata) {
            return false;
        }

        cerr.flush();
        cout << current << left << setw(16) << ' ' << endl;
        TFile file(current.GetName(), current.GetSize());
        m_Dlcbdata->Append(&file);
        return true;
    }

private:
    CDownloadCallbackData* m_Dlcbdata;
};


class CProcessor
{
public:
    virtual         ~CProcessor() { }

    virtual size_t   Run       (void) = 0;
    virtual void     Stop      (void) = 0;
    virtual istream* GetIStream(void) const { return m_Stream; }

protected:
    CProcessor(CProcessor* prev, istream* is, CDownloadCallbackData* dlcbdata)
        : m_Prev(prev), m_Stream(is),
          m_Dlcbdata(dlcbdata ? dlcbdata : prev ? prev->m_Dlcbdata : 0)
    { }

    CProcessor*            m_Prev;
    istream*               m_Stream;
    CDownloadCallbackData* m_Dlcbdata;
};


class CNullProcessor : public CProcessor
{
public:
    CNullProcessor(istream& input,   CDownloadCallbackData* dlcbdata = 0)
        : CProcessor(0, &input, dlcbdata)
    { }
    CNullProcessor(CProcessor& prev, CDownloadCallbackData* dlcbdata = 0)
        : CProcessor(&prev, 0, dlcbdata)
    { }
    ~CNullProcessor() { Stop(); }

    size_t Run (void);
    void   Stop(void) { m_Stream = 0; delete m_Prev; m_Prev = 0; }
};


class CListProcessor : public CNullProcessor
{
public:
    CListProcessor(CProcessor& prev, CDownloadCallbackData* dlcbdata = 0)
        : CNullProcessor(prev, dlcbdata)
    { m_Stream = m_Prev->GetIStream(); }
    ~CListProcessor() { Stop(); }

    size_t           Run       (void);
    virtual istream* GetIStream(void) const { return 0; }
};


class CGunzipProcessor : public CNullProcessor
{
public:
    CGunzipProcessor(CProcessor& prev, CDownloadCallbackData* dlcbdata = 0);
    ~CGunzipProcessor() { Stop(); }

    void   Stop(void);
};


class CUntarProcessor : public CProcessor
{
public:
    CUntarProcessor(CProcessor& prev, CDownloadCallbackData* dlcbdata = 0);
    ~CUntarProcessor() { Stop(); }

    size_t Run (void);
    void   Stop(void);

private:
    CTar* m_Tar;
};


size_t CNullProcessor::Run(void)
{
    if (!m_Stream  ||  !m_Stream->good()  ||  !m_Dlcbdata) {
        return 0;
    }

    Uint8 filesize = 0;
    do {
        static char buf[10240];
        m_Stream->read(buf, sizeof(buf));
        filesize += m_Stream->gcount();
    } while (*m_Stream);
    CTar::TFile file = make_pair(m_Dlcbdata->GetFilename(), filesize);
    m_Dlcbdata->Append(&file);
    return 1;
}


size_t CListProcessor::Run(void)
{
    if (!m_Stream  ||  !m_Stream->good()  ||  !m_Dlcbdata) {
        return 0;
    }

    auto_ptr<CTar::TEntries> filelist;
    size_t n = 0;
    do {
        string line;
        getline(*m_Stream, line);
        if (!line.empty()  &&  NStr::EndsWith(line, '\r')) {
            line.resize(line.size() - 1);
        }
        if (!line.empty()) {
            cerr.flush();
            cout << left << setw(60) << line << endl;
            CTar::TFile file = make_pair(line, (Uint8) 0);
            m_Dlcbdata->Append(&file);
            n++;
        }
    } while (*m_Stream);
    return n;
}


CGunzipProcessor::CGunzipProcessor(CProcessor&            prev,
                                   CDownloadCallbackData* dlcbdata)
    : CNullProcessor(prev, dlcbdata)
{
    istream* is = m_Prev->GetIStream();
    // Build on-the-fly ungzip stream on top of another stream
    m_Stream = is ? new CDecompressIStream(*is,CCompressStream::eGZipFile) : 0;
}


void CGunzipProcessor::Stop(void)
{
    delete m_Stream;
    m_Stream = 0;
    delete m_Prev;
    m_Prev = 0;
}


CUntarProcessor::CUntarProcessor(CProcessor&            prev,
                                 CDownloadCallbackData* dlcbdata)
    : CProcessor(&prev, 0, dlcbdata)
{
    istream* is = m_Prev->GetIStream();
    // Build streaming [un]tar on top of another stream
    m_Tar = is ? new CTarCheckpointed(*is, m_Dlcbdata) : 0;
}


size_t CUntarProcessor::Run(void)
{
    if (!m_Tar) {
        return 0;
    }

    auto_ptr<CTar::TEntries> filelist;
    try {  // NB: CTar *loves* exceptions, for some weird reason :-/
        filelist = m_Tar->List();  // NB: can be tar.Extract() as well
    } NCBI_CATCH_ALL("TAR Error");
    return filelist.get() ? filelist->size() : 0;
}


void CUntarProcessor::Stop(void)
{
    delete m_Tar;
    m_Tar = 0;
    delete m_Prev;
    m_Prev = 0;
}


static bool s_IsATTY(void)
{
#if   defined(NCBI_OS_UNIX)
    return  isatty(STDERR_FILENO)   ? true : false;
#elif defined(NCBI_OS_MSWIN)
    return _isatty(_fileno(stdout)) ? true : false;
#else
    return false/*safe choice*/;
#endif // NCBI_OS
}


extern "C" {
static EIO_Status x_FtpCallback(void* data, const char* cmd, const char* arg)
{
    if (NStr::strncasecmp(cmd, "SIZE", 4) != 0  &&
        NStr::strncasecmp(cmd, "RETR", 4) != 0) {
        return eIO_Success;
    }

    Uint8 val = NStr::StringToUInt8(arg, NStr::fConvErr_NoThrow);
    if (!val) {
        ERR_POST("Cannot read file size " << arg << " in "
                 << CTempString(cmd, 4) << " command response");
        return eIO_Unknown;
    }

    CDownloadCallbackData* dlcbdata
        = reinterpret_cast<CDownloadCallbackData*>(data);

    Uint8 size = dlcbdata->GetSize();
    if (!size) {
        size = val;
        ERR_POST(Info << "Got file size in "
                 << CTempString(cmd, 4) << " command: "
                 << NStr::UInt8ToString(size) << " byte" << &"s"[size == 1]);
        dlcbdata->SetSize(size);
    } else if (size != val) {
        ERR_POST(Fatal << "File size " << arg << " in "
                 << CTempString(cmd, 4) << " command mismatches "
                 << NStr::UInt8ToString(size));
    } else {
        ERR_POST(Info << "File size " << arg << " in "
                 << CTempString(cmd, 4) << " command matches");
    }
    return eIO_Success;
}


static void x_ConnectionCallback(CONN conn, ECONN_Callback type, void* data)
{
    bool update;

    if (type != eCONN_OnClose  &&  !s_Signaled) {
        // Reinstate the callback right away
        SCONN_Callback cb = { x_ConnectionCallback, data };
        CONN_SetCallback(conn, type, &cb, 0);
        update = false;
    } else {
        update = true;
    }

    CDownloadCallbackData* dlcbdata
        = reinterpret_cast<CDownloadCallbackData*>(data);
    Uint8  pos  = CONN_GetPosition(conn, eIO_Read);
    double time = dlcbdata->GetElapsed(update);
    Uint8  size = dlcbdata->GetSize();

    if (type == eCONN_OnClose) {
        // Callback up the chain
        if (dlcbdata->CB()->func) {
            dlcbdata->CB()->func(conn, type, dlcbdata->CB()->data);
        }
        // Finalize the filelist
        dlcbdata->Append();
    } else if (s_Signaled) {
        CONN_Cancel(conn);
    } else if (s_Throttler) {
        SleepMilliSec(s_Throttler);
    }

    if (!update  &&  !time) {
        return;
    }

    if (s_IsATTY()) {
        cout.flush();
        if (!size) {
            cerr << "Downloaded " << NStr::UInt8ToString(pos)
                 << "/unknown"
                " in " << fixed << setprecision(2) << time << 's';
        } else {
            double percent = (pos * 100.0) / size;
            cerr << "Downloaded " << NStr::UInt8ToString(pos)
                 << '/' << NStr::UInt8ToString(size)
                 << " (" << fixed << setprecision(2) << percent << "%)"
                " in " << fixed << setprecision(2) << time << 's';
            if (time) {
                double kbps = pos / time / 1024.0;
                cerr << " (" << fixed << setprecision(2) << kbps << "KB/s)";
                if (pos) {
                    double eta = time * size / pos - time;
                    if (eta > 0) {
                        cerr << " ETA: " <<
                            (eta > 24 * 3600
                             ? CTimeSpan(eta).AsString("d") + "+ day(s)"
                             : CTimeSpan(eta).AsString("h:m:s"));
                    }
                }
            }
        }
        cerr << left << setw(16) << ' ' << '\r' << flush;
    }

    if (type == eCONN_OnClose) {
        cerr << endl << "Connection closed" << endl;
    } else if (s_Signaled) {
        cerr << endl << "Canceled" << endl;
    }
}
}


static void s_TryAskFtpFilesize(iostream& ios, const char* filename)
{
    _DEBUG_ARG(size_t testval);
    ios << "SIZE " << filename << endl;
    // If the SIZE command is not understood, the file size can
    // be captured at download start (RETR) if reported by the server
    // in a compatible format (there's the callback set for that, too).
    _ASSERT(!(ios >> testval));  //  NB: we're getting a callback instead
    ios.clear();
}


static void s_InitiateFtpRetrieval(CConn_IOStream& s,
                                   const char*     name,
                                   const STimeout* timeout)
{
    // RETR must be understood by all FTP implementations
    // LIST command obtains non-machine readable output
    // NLST command obtains a filename per line output (having the filelist,
    ///     one can use SIZE and MDTM on each entry to get details about it)
    const char* cmd = NStr::EndsWith(name, '/') ? "LIST " : "RETR ";
    s << cmd << name << endl;
    EIO_Status status = CONN_Wait(s.GetCONN(), eIO_Read, timeout);
    _ASSERT(status != eIO_InvalidArg);
    if (status != eIO_Success) {
        s.clear(IOS_BASE::badbit);
    }
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    typedef enum {
        fProcessor_Null   = 0,  // Discard read data
        fProcessor_List   = 4,  // List files line-by-line
        fProcessor_Untar  = 1,  // Untar then discard read data
        fProcessor_Gunzip = 2   // Decompress then discard read data
    } FProcessor;
    typedef unsigned int TProcessor;

    // Setup error posting
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Trace);
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    CONNECT_Init(0);

    const char* url = argc > 1  &&  *argv[1] ? argv[1] : kDefaultTestURL;
    if (argc > 2) {
        s_Throttler = NStr::StringToInt(argv[2]);
    }

    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    net_info->path[0] = '\0';
    net_info->args[0] = '\0';
    net_info->http_referer = 0;
    ConnNetInfo_SetUserHeader(net_info, 0);

    if (!ConnNetInfo_ParseURL(net_info, url)) {
        ERR_POST(Fatal << "Cannot parse URL \"" << url << '"');
    }

    if        (net_info->scheme == eURL_Unspec) {
        if (NStr::strcasecmp(net_info->host, DEF_CONN_HOST) == 0) {
            strcpy(net_info->host, "ftp.ncbi.nlm.nih.gov");
        }
        net_info->scheme = eURL_Ftp;
    } else if (net_info->scheme != eURL_Ftp) {
        ERR_POST(Fatal << "URL scheme must be FTP (ftp://) if specified");
    }
    if (!*net_info->user) {
        ERR_POST(Warning << "Username not provided, defaulted to `ftp'");
        strcpy(net_info->user, "ftp");
    }

    ConnNetInfo_LogEx(net_info, eLOG_Note, CORE_GetLOG());

    url = ConnNetInfo_URL(net_info);
    _ASSERT(url);

    TFTP_Flags flags = 0;
    if        (net_info->debug_printout == eDebugPrintout_Some) {
        flags |= fFTP_LogControl;
    } else if (net_info->debug_printout == eDebugPrintout_Data) {
        flags |= fFTP_LogAll;
    }
    if        (net_info->req_method == eReqMethod_Get) {
        flags |= fFTP_UsePassive;
    } else if (net_info->req_method == eReqMethod_Post) {
        flags |= fFTP_UseActive;
    }
    flags     |= fFTP_UseFeatures | fFTP_NotifySize;

    const char* filename = net_info->path;
    if (filename[0] == '/'  &&  filename[1]) {
        filename++;
    }

    CDownloadCallbackData dlcbdata(filename);
    SFTP_Callback ftpcb = { x_FtpCallback, &dlcbdata };

    CConn_FtpStream ftp(net_info->host,
                        net_info->user,
                        net_info->pass,
                        kEmptyStr/*path*/,
                        net_info->port,
                        flags,
                        &ftpcb,
                        net_info->timeout);

    TProcessor proc = fProcessor_Null;
    if        (NStr::EndsWith(filename, ".tgz",    NStr::eNocase)  ||
               NStr::EndsWith(filename, ".tar.gz", NStr::eNocase)) {
        proc |= fProcessor_Gunzip | fProcessor_Untar;
    } else if (NStr::EndsWith(filename, ".gz",     NStr::eNocase)) {
        proc |= fProcessor_Gunzip;
    } else if (NStr::EndsWith(filename, ".tar",    NStr::eNocase)) {
        proc |= fProcessor_Untar;
    } else if (NStr::EndsWith(filename, '/')) {
        proc |= fProcessor_List;
    }

    s_TryAskFtpFilesize(ftp, net_info->path);
    Uint8 size = dlcbdata.GetSize();
    if (size) {
        ERR_POST(Info << "Downloading " << url << ", "
                 << NStr::UInt8ToString(size) << " byte" << &"s"[size == 1]
                 << ", processor 0x" << hex << proc);
    } else {
        ERR_POST(Info << "Downloading " << url
                 << ", processor 0x" << hex << proc);
    }
    s_InitiateFtpRetrieval(ftp, net_info->path, net_info->timeout);

    ConnNetInfo_Destroy(net_info);
    free((void*) url);

#ifdef NCBI_OS_UNIX
    signal(SIGINT,  s_Interrupt);
    signal(SIGTERM, s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#endif // NCBI_OS_UNIX

    // NB: Can use "CONN_GetPosition(ftp.GetCONN(), eIO_Open)" to clear
    _ASSERT(!CONN_GetPosition(ftp.GetCONN(), eIO_Read));
    // Set both read and close callbacks
    SCONN_Callback conncb = { x_ConnectionCallback, &dlcbdata };
    CONN_SetCallback(ftp.GetCONN(), eCONN_OnRead,  &conncb, 0/*dontcare*/);
    CONN_SetCallback(ftp.GetCONN(), eCONN_OnClose, &conncb, dlcbdata.CB());

    CProcessor* processor = new CNullProcessor(ftp, &dlcbdata);
    if (proc & fProcessor_Gunzip) {
        processor = new CGunzipProcessor(*processor);
    }
    if (proc & fProcessor_Untar) {
        processor = new CUntarProcessor(*processor);
    }
    if (proc & fProcessor_List) {
        processor = new CListProcessor(*processor);
    }

    // Process stream data
    size_t files = processor->Run();

    // These should not matter, and can be issued in any order
    ftp.Close();  // ...so do the "wrong" order on purpose to prove it works!
    delete processor;

    // Conclude the test
    Uint8 totalsize = 0;
    ITERATE(CTar::TFiles, it, dlcbdata.Filelist()) {
        totalsize += it->second;
    }

    ERR_POST(Info << "Total downloaded " << dlcbdata.Filelist().size()
             << " file" << &"s"[dlcbdata.Filelist().size() == 1] << " in "
             << CTimeSpan(dlcbdata.GetElapsed() + 0.5).AsString("h:m:s")
             << "; combined file size " << NStr::UInt8ToString(totalsize));

    if (!files  ||  !dlcbdata.Filelist().size()) {
        // Interrupted or an error has occurred
        ERR_POST("Final file list is empty");
        return 1;
    }
    if (files != dlcbdata.Filelist().size()) {
        // NB: It may be so for an "updated" archive...
        ERR_POST(Warning << "Final file list size mismatch: " << files);
        return 1;
    }
    return 0;
}
