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
 * Author: Frank Ludwig
 *
 * File Description:
 *   Basic reader interface
 *
 */

#ifndef OBJTOOLS_READERS___MESSAGELISTENER__HPP
#define OBJTOOLS_READERS___MESSAGELISTENER__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
class IMessageListener
//  ============================================================================
{
public:
    virtual ~IMessageListener() {}
    /// Store error in the container, and 
    /// return true if error was stored fine, and
    /// return false if the caller should terminate all further processing.
    ///
    virtual bool
    PutError(
        const ILineError& ) = 0;
    
    /// 0-based error retrieval.
    virtual const ILineError&
    GetError(
        size_t ) =0;
        
    /// Return number of errors seen so far.
    virtual size_t
    Count() const =0;
    
    /// Returns the number of errors seen so far at the given severity.
    virtual size_t
    LevelCount(
        EDiagSev ) =0;
            
    virtual void
    ClearAll() =0;

    virtual void
    PutProgress(
        const string & sMessage,
        const Uint8 iNumDone = 0,
        const Uint8 iNumTotal = 0 ) = 0;
};
            
//  ============================================================================
class CMessageListenerBase:
//  ============================================================================
    public CObject, public IMessageListener
{
public:
    CMessageListenerBase() {};
    virtual ~CMessageListenerBase() {};
    
public:
    size_t
    Count() const { return m_Errors.size(); };
    
    virtual size_t
    LevelCount(
        EDiagSev eSev ) {
        
        size_t uCount( 0 );
        for ( size_t u=0; u < Count(); ++u ) {
            if ( m_Errors[u].Severity() == eSev ) ++uCount;
        }
        return uCount;
    };
    
    void
    ClearAll() { m_Errors.clear(); };
    
    const ILineError&
    GetError(
        size_t uPos ) { return m_Errors[ uPos ]; };
    
    virtual void Dump(
        std::ostream& out )
    {
        if ( m_Errors.size() ) {
            std::vector<CLineError>::iterator it;
            for ( it= m_Errors.begin(); it != m_Errors.end(); ++it ) {
                it->Dump( out );
                out << endl;
            }
        }
        else {
            out << "(( no errors ))" << endl;
        }
    };

    virtual void
    PutProgress(
        const string & sMessage,
        const Uint8 iNumDone = 0,
        const Uint8 iNumTotal = 0 ) 
    {
        // default is to turn the progress into an info-level
        // message, but child classes are free to handle progress
        // messages however they like

        CNcbiOstrstream msg_strm;
        msg_strm << sMessage;
        if( iNumDone > 0 || iNumTotal > 0 ) {
            msg_strm << " (" << iNumDone;
            if( iNumTotal > 0 ) {
                msg_strm << " of " << iNumTotal;
            }
            msg_strm << " done)";
        }

        CLineError progress_msg(
            ILineError::eProblem_ProgressInfo,
            eDiag_Info,
            CNcbiOstrstreamToString(msg_strm),
            0 // line
            );

        PutError(progress_msg);
    }

private:
    // private so later we can change the structure if
    // necessary (e.g. to have indexing and such to speed up
    // level-counting)
    std::vector< CLineError > m_Errors;

protected:

    // Child classes should use this to store errors
    // into m_Errors
    void StoreError(const ILineError& err)
    {
        m_Errors.push_back( 
            CLineError( err.Problem(), err.Severity(), err.SeqId(), err.Line(), 
                err.FeatureName(), err.QualifierName(), err.QualifierValue()) );
        ITERATE( ILineError::TVecOfLines, line_it, err.OtherLines() ) {
            m_Errors.back().AddOtherLine(*line_it);
        }
    }
};

//  ============================================================================
class CMessageListenerLenient:
//
//  Accept everything.
//  ============================================================================
    public CMessageListenerBase
{
public:
    CMessageListenerLenient() {};
    ~CMessageListenerLenient() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        StoreError(err);
        return true;
    };
};        

//  ============================================================================
class CMessageListenerStrict:
//
//  Don't accept any errors, at all.
//  ============================================================================
    public CMessageListenerBase
{
public:
    CMessageListenerStrict() {};
    ~CMessageListenerStrict() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        StoreError(err);
        return false;
    };
};        

//  ===========================================================================
class CMessageListenerCount:
//
//  Accept up to <<count>> errors, any level.
//  ===========================================================================
    public CMessageListenerBase
{
public:
    CMessageListenerCount(
        size_t uMaxCount ): m_uMaxCount( uMaxCount ) {};
    ~CMessageListenerCount() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        StoreError(err);
        return (Count() < m_uMaxCount);
    };    
protected:
    size_t m_uMaxCount;
};

//  ===========================================================================
class CMessageListenerLevel:
//
//  Accept evrything up to a certain level.
//  ===========================================================================
    public CMessageListenerBase
{
public:
    CMessageListenerLevel(
        int iLevel ): m_iAcceptLevel( iLevel ) {};
    ~CMessageListenerLevel() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        StoreError(err);
        return (err.Severity() <= m_iAcceptLevel);
    };    
protected:
    int m_iAcceptLevel;
};

//  ===========================================================================
class CMessageListenerWithLog:
//
//  Accept everything, and besides storing all errors, post them.
//  ===========================================================================
    public CMessageListenerBase
{
public:
    CMessageListenerWithLog(const CDiagCompileInfo& info)
        : m_Info(info) {};
    ~CMessageListenerWithLog() {};

    bool
    PutError(
        const ILineError& err )
    {
        CNcbiDiag(m_Info, err.Severity(),
                  eDPF_Log | eDPF_IsMessage).GetRef()
           << err.Message() << Endm;

        StoreError(err);
        return true;
    };

private:
    const CDiagCompileInfo m_Info;
};

END_SCOPE(objects)

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___MESSAGELISTENER__HPP
