#ifndef DBAPI_UTILS___BLOBSTORE__HPP
#define DBAPI_UTILS___BLOBSTORE__HPP

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
 * Author:  Vladimir Soussov
 *
 * File Description:  Blob store classes
 *
 */

#include <util/reader_writer.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

enum ECompressMethod {
    eNone,
    eZLib,
    eBZLib
};

class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobReader : public IReader {
public:
    CBlobReader(CDB_Result* res, I_BaseCmd* cmd= 0, I_Connection* con= 0) {
        m_Res= res;
        m_Cmd= cmd;
        m_Con= con;
        m_ItemNum= 0;
        m_AllDone= false;
    }
    /// Read as many as count bytes into a buffer pointed
    /// to by buf argument.  Store the number of bytes actually read,
    /// or 0 on EOF or error, via the pointer "bytes_read", if provided.
    /// Special case:  if count passed as 0, then the value of
    /// buf is ignored, and the return value is always eRW_Success, but
    /// no change is actually done to the state of input device.
    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    /// Return the number of bytes ready to be read from input
    /// device without blocking.  Return 0 if no such number is
    /// available (in case of an error or EOF).
    virtual ERW_Result PendingCount(size_t* count);

    virtual ~CBlobReader();

private:
    CBlobReader();
    CDB_Result* m_Res;
    I_BaseCmd* m_Cmd;
    I_Connection* m_Con;
    int m_ItemNum;
    bool m_AllDone;
};


class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT ItDescriptorMaker {
public:
    virtual bool Init(CDB_Connection* con)= 0;
    virtual I_ITDescriptor& ItDescriptor(void)= 0;
    virtual bool Fini(void)= 0;
    virtual ~ItDescriptorMaker(){};
};


class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobWriter : public IWriter
{
public:
    enum EFlags {
        fLogBlobs = 0x1,
        fOwnDescr= 0x2,
        fOwnCon= 0x4,
        fOwnAll= fOwnDescr + fOwnCon
    };

    typedef int TFlags;

    CBlobWriter(CDB_Connection* con,
                ItDescriptorMaker*    desc_func,
                size_t          image_limit= 0x7FFFFFFF,
                TFlags          flags= 0);
    /// Write up to count bytes from the buffer pointed to by
    /// buf argument onto output device.  Store the number
    /// of bytes actually written, or 0 if either count was
    /// passed as 0 (buf is ignored in this case) or an error occured,
    /// via the "bytes_written" pointer, if provided.
    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    /// Flush pending data (if any) down to output device.
    virtual ERW_Result Flush(void);

    virtual ~CBlobWriter();

private:
    CBlobWriter();
    bool storeBlob(void);
    CDB_Image m_Blob;
    ItDescriptorMaker *m_dMaker;
    size_t m_Limit;
    CDB_Connection* m_Con;
    bool m_LogIt;
    bool m_DelDesc;
    bool m_DelCon;
};


class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobRetriever {
public:
    CBlobRetriever(I_DriverContext* pCntxt,
                   const string& server,
                   const string& user,
                   const string& passwd,
                   const string& query);
    bool IsReady() const {
        return m_IsGood;
    }
    bool Dump(ostream& s, ECompressMethod cm= eNone);
    ~CBlobRetriever();

private:
    CBlobRetriever();
    CDB_Connection* m_Conn;
    CDB_LangCmd* m_Cmd;
    CDB_Result* m_Res;
    bool m_IsGood;
};


class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobLoader {
public:
    CBlobLoader(I_DriverContext* pCntxt,
                const string& server,
                const string& user,
                const string& passwd,
                ItDescriptorMaker* d_maker
                );
    bool IsReady() const {
        return m_IsGood;
    }
    bool Load(istream& s, ECompressMethod cm= eNone,
              size_t image_limit= 0, bool log_it= false);
    ~CBlobLoader() {
        if(m_Conn) delete m_Conn;
    }

private:
    CBlobLoader();
    CDB_Connection* m_Conn;
    ItDescriptorMaker* m_dMaker;
    bool m_IsGood;
};


/***************************************************************************************
 * The SimpleBlobStore is a ready to use implementation of ItDescriptorMaker
 * it uses a table of the form:
 * create table TABLE_NAME (
 * ID varchar(n),
 * NUM int,
 * DATA1 image NULL, ... DATAn image NULL)
 *
 */
class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CMY_ITDescriptor : public CDB_ITDescriptor {
public:
    CMY_ITDescriptor(const string& table_name) :
        CDB_ITDescriptor(table_name, kEmptyStr, kEmptyStr)
    {};
    void SetColumn(const string& col_name) {
        m_ColumnName= col_name;
    }
    void SetSearchConditions(const string& s) {
        m_SearchConditions= s;
    }
};


class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CSimpleBlobStore
    : public ItDescriptorMaker {
public:
    CSimpleBlobStore(const string& table_name,
                     const string& key_col_name,
                     const string& num_col_name,
                     const string blob_column[],
                     bool is_text= false);
    void SetKey(const string& key) {
        if(!key.empty())
            m_Key= key;
    }
    virtual bool Init(CDB_Connection* con);
    virtual I_ITDescriptor& ItDescriptor(void);
    virtual bool Fini(void);
    virtual ~CSimpleBlobStore();

protected:
    string m_TableName;
    string m_KeyColName;
    string m_NumColName;
    string m_sCMD;
    string* m_DataColName;
    CDB_Connection* m_Con;
    CDB_LangCmd*    m_Cmd;
    int m_nofDataCols;
    int m_ImageNum;
    CDB_VarChar m_Key;
    CDB_Int m_RowNum;
    CMY_ITDescriptor m_Desc;
};


/***************************************************************************************
 * CBlobStoreBase - the abstract base interface to deal with reading and writing
 * the image/text data from a C++ application.
 */

class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobStoreBase {
public:

    enum
    {
        // Default image limit
        IMAGE_LIMIT_16MB = 0x1000000
    };


    bool Exists(const string& blob_id);
    //user has to delete istream
    istream* OpenForRead(const string& blob_id);
    // user has to delete ostream
    ostream* OpenForWrite(const string& blob_id);
    void Delete(const string& blob_id);

    size_t GetImageLimit() const { return m_Limit; }

    ECompressMethod getCM() const { return m_Cm; }
    void setCM(ECompressMethod cm) { m_Cm = cm; }

    const string& getTableName() const { return m_Table; }
    const string& getKeyColName() const { return m_KeyColName; }
    const string& getNumColName() const { return m_NumColName; }

    virtual ~CBlobStoreBase();

protected:
    CBlobStoreBase(const string& table_name,
                   ECompressMethod cm = eNone,
                   size_t image_limit = IMAGE_LIMIT_16MB,
                   bool log_it = false);

    void ReadTableDescr();
    void SetTableDescr(const string& tableName,
                       const string& keyColName,
                       const string& numColName,
                       const string* blobColNames,
                       unsigned nofBC,
                       bool isText = false);
    void SetTextSizeServerSide(CDB_Connection* pConn,
                               size_t textSize = 2147483647);
    virtual void GenReadQuery();
    virtual CDB_Connection* GetConn() = 0;
    // Returns true if connection should be deleted.
    virtual bool ReleaseConn(CDB_Connection*) = 0;

private:
    string m_Table;
    ECompressMethod m_Cm;
    size_t m_Limit;
    bool m_LogIt;
    bool m_IsText;
    string m_KeyColName;
    string m_NumColName;
    string m_ReadQuery;
    string* m_BlobColumn;
    unsigned m_NofBC;
};


/***************************************************************************************
 * CBlobStoreStatic - the simple interface to deal with reading and writing
 * the image/text data from a C++ application.
 * It uses connection to DB from an external pool.
 */

class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobStoreStatic
    : public CBlobStoreBase
{
public:
    CBlobStoreStatic(CDB_Connection* pConn,
                     const string& table_name,
                     ECompressMethod cm = eNone,
                     size_t image_limit = IMAGE_LIMIT_16MB,
                     bool log_it = false);

    CBlobStoreStatic(CDB_Connection* pConn,
                     const string& tableName,
                     const string& keyColName,
                     const string& numColName,
                     const string* blobColNames,
                     unsigned nofBC,
                     bool isText = false,
                     ECompressMethod cm = eNone,
                     size_t image_limit = IMAGE_LIMIT_16MB,
                     bool log_it = false);

    virtual ~CBlobStoreStatic();

protected:
    virtual CDB_Connection* GetConn();
    virtual bool ReleaseConn(CDB_Connection*) { return false; }

private:
    CDB_Connection* m_pConn;
};


/***************************************************************************************
 * CBlobStoreDynamic - the simple interface to deal with reading and writing
 * the image/text data from a C++ application.
 * It uses an internal connections pool and ask pool for a connection each time before
 * connection use.
 */

class NCBI_DBAPIUTIL_BLOBSTORE_EXPORT CBlobStoreDynamic
    : public CBlobStoreBase
{
public:
    CBlobStoreDynamic(I_DriverContext* pCntxt,
                      const string& server,
                      const string& user,
                      const string& passwd,
                      const string& table_name,
                      ECompressMethod cm = eNone,
                      size_t image_limit = IMAGE_LIMIT_16MB,
                      bool log_it = false);

    virtual ~CBlobStoreDynamic();

protected:
    virtual CDB_Connection* GetConn();
    virtual bool ReleaseConn(CDB_Connection*);

private:
    I_DriverContext* m_Cntxt;
    string m_Server;
    string m_User;
    string m_Passwd;
    string m_Pool;
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/11/24 19:11:07  ivanovsk
 * Set TEXTSIZE on server in CBlobStore classes.
 *
 * Revision 1.7  2004/10/25 14:32:08  ivanovsk
 * Replace #define of default image size by enum.
 *
 * Revision 1.6  2004/10/22 11:40:17  ivanov
 * Replaced static const member CBlobStoreBase::g_16MB with
 * #define IMAGE_LIMIT_16MB.
 *
 * Revision 1.5  2004/10/21 18:33:18  ivanovsk
 * Add methods to take blob key column name and
 * num column name.
 *
 * Revision 1.4  2004/10/20 18:21:28  ivanov
 * Added export specifier to all class declaratons
 *
 * Revision 1.3  2004/10/18 23:17:28  ivanovsk
 *
 * Rename class CBlobStore to CBlobStoreBase and make it abstract. Add two descendants CBlobStoreStatic and CBlobStoreDynamic for different types of DB connection usage.
 *
 * Revision 1.2  2004/05/24 19:40:47  soussov
 * adds CBlobStore implementation
 *
 * Revision 1.1  2004/05/03 16:47:10  soussov
 * initial commit
 *
 * ===========================================================================
 */

#endif  /* DBAPI_UTILS___BLOBSTORE__HPP */
