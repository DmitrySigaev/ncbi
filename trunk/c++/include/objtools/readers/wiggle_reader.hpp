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
 *   WIGGLE file reader
 *
 */

#ifndef OBJTOOLS_READERS___WIGGLEREADER__HPP
#define OBJTOOLS_READERS___WIGGLEREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CWiggleSet;
class CWiggleRecord;
class CIdMapper;

//  ----------------------------------------------------------------------------
enum CWiggleLineType
//  ----------------------------------------------------------------------------
{
    TYPE_NONE,
    TYPE_COMMENT,
    TYPE_TRACK,
    TYPE_DECLARATION_VARSTEP,
    TYPE_DECLARATION_FIXEDSTEP,
    TYPE_DATA_BED,
    TYPE_DATA_VARSTEP,
    TYPE_DATA_FIXEDSTEP
};


//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CWiggleReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    //
    //  object management:
    //
public:
    CWiggleReader( 
        int =fDefaults );
        
    CWiggleReader( 
        const CArgs& );
        
    virtual ~CWiggleReader();
    
    //
    //  object interface:
    //
public:
    enum EWiggleFlags {
        fDefaults = 0
    };
    typedef int TFlags;

    virtual void Read( 
        CNcbiIstream&, 
        CRef<CSeq_annot>& );

    virtual void Dump(
        CNcbiOstream& );
        
    //
    //  class interface:
    //
    static bool VerifyFormat(
        CNcbiIstream& );

    static void Tokenize(
        const string&,
        const string&,
        vector< string >& );

    //
    //  helpers:
    //
protected:
    bool x_ReadTrackData(
        CNcbiIstream&,
        string&,
        CWiggleRecord& );

    bool x_ReadGraphData(
        CNcbiIstream&,
        string&,
        CWiggleRecord& );

    bool x_IsCommentLine(
        const string& );

    bool x_ReadLine(
        CNcbiIstream&,
        string& );

    unsigned int x_GetLineType(
        const string& );

    void x_UpdateWiggleSet();
    
    //
    //  data:
    //
protected:
    unsigned int m_uCurrentRecordType;

    CIdMapper* m_pIdMapper;    
    CWiggleSet* m_pSet;
    unsigned int m_uIdMode;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___WIGGLEREADER__HPP
