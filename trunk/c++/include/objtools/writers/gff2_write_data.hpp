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
 *   GFF3 transient data structures
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF2DATA__HPP
#define OBJTOOLS_WRITERS___GFF2DATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


//  ----------------------------------------------------------------------------
class CGffWriteRecord
//  ----------------------------------------------------------------------------
    : public CObject
{
public:
    typedef map<string, string> TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGffWriteRecord();
    CGffWriteRecord(
        const CGffWriteRecord& );
    virtual ~CGffWriteRecord();

    //
    //  Input/output:
    //
    virtual bool AssignLocation(
        const CSeq_interval& );

    bool MergeRecord(
        const CGffWriteRecord& );

    bool AssignType(
        const string& strType ) {
        m_strType = strType;
        return true;
    };

    bool AssignSequenceNumber(
        unsigned int,
        const string& = "" );

    virtual string StrType() const;
    virtual string StrAttributes() const;
    virtual string StrId() const;
    virtual string StrSource() const;
    virtual string StrSeqStart() const;
    virtual string StrSeqStop() const;
    virtual string StrScore() const;
    virtual string StrStrand() const;
    virtual string StrPhase() const;
    virtual string StrStructibutes() const { return ""; };

    const TAttributes& Attributes() const { 
        return m_Attributes; 
    };
    bool GetAttribute(
        const string&,
        string& ) const;

protected:
    static bool x_NeedsQuoting(
        const string& );

    virtual void x_PriorityProcess(
        const string&,
        map<string, string >&,
        string& ) const;

    string m_strId;
    size_t m_uSeqStart;
    size_t m_uSeqStop;
    string m_strSource;
    string m_strType;
    double* m_pdScore;
    ENa_strand* m_peStrand;
    unsigned int* m_puPhase;
    string m_strAttributes;    
    TAttributes m_Attributes;
};

//  ----------------------------------------------------------------------------
class CGffWriteRecordFeature
//  ----------------------------------------------------------------------------
    : public CGffWriteRecord
{
public:
    virtual bool AssignFromAsn(
        CMappedFeat );

protected:
    virtual bool x_AssignType(
        CMappedFeat );
    virtual bool x_AssignSeqId(
        CMappedFeat );
    virtual bool x_AssignStart(
        CMappedFeat );
    virtual bool x_AssignStop(
        CMappedFeat );
    virtual bool x_AssignSource(
        CMappedFeat );
    virtual bool x_AssignScore(
        CMappedFeat );
    virtual bool x_AssignStrand(
        CMappedFeat );
    virtual bool x_AssignPhase(
        CMappedFeat );
    virtual bool x_AssignAttributes(
        CMappedFeat );
    static string x_FeatIdString(
        const CFeat_id& id );
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF2DATA__HPP
