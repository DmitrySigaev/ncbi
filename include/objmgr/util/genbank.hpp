#ifndef GENBANK__HPP
#define GENBANK__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   Code to write Genbank/Genpept flat-file records.
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// forward declarations
class CBioSource;
class CFeat_id;
class COrg_ref;
class CSeq_entry;
class CSeq_feat;
class CSeq_loc;
class CSeq_interval;

class NCBI_XOBJUTIL_EXPORT CGenbankWriter
{
public:
    enum EFormat {
        eFormat_Genbank,
        eFormat_Genpept
    };
    enum EVersion {
        // Genbank 127.0, to be released December 2001, will have a new
        // format for the LOCUS line, with more space for most fields.
        eVersion_pre127,
        eVersion_127 = 127
    };
    
    CGenbankWriter(CNcbiOstream& stream, CScope& scope,
                   EFormat format = eFormat_Genbank,
                   EVersion version = eVersion_pre127) :
        m_Stream(stream), m_Scope(scope), m_Format(format), m_Version(version)
        { SetParameters(); }

    bool Write(const CSeq_entry& entry) const; // returns true on success

private:
    // useful constants
    static const unsigned int sm_KeywordWidth;
    static const unsigned int sm_LineWidth;
    static const unsigned int sm_DataWidth;
    static const unsigned int sm_FeatureNameIndent;
    static const unsigned int sm_FeatureNameWidth;

    void SetParameters(void); // sets formatting parameters (below)

    bool WriteLocus     (const CBioseq_Handle& handle) const;
    bool WriteDefinition(const CBioseq_Handle& handle) const;
    bool WriteAccession (const CBioseq_Handle& handle) const;
    bool WriteID        (const CBioseq_Handle& handle) const;
    bool WriteVersion   (const CBioseq_Handle& handle) const;
    bool WriteDBSource  (const CBioseq_Handle& handle) const;
    bool WriteKeywords  (const CBioseq_Handle& handle) const;
    bool WriteSegment   (const CBioseq_Handle& handle) const;
    bool WriteSource    (const CBioseq_Handle& handle) const;
    bool WriteReference (const CBioseq_Handle& handle) const;
    bool WriteComment   (const CBioseq_Handle& handle) const;
    bool WriteFeatures  (const CBioseq_Handle& handle) const;
    bool WriteSequence  (const CBioseq_Handle& handle) const;

    void Wrap(const string& keyword, const string& contents,
              unsigned int indent = sm_KeywordWidth) const;
    void WriteFeatureLocation(const string& name, const CSeq_loc& location,
                              const CBioseq& default_seq) const;
    void WriteFeatureLocation(const string& name, const string& location)
        const;
    void WriteFeatureQualifier(const string& qual) const;

    void FormatIDPrefix(const CSeq_id& id, const CBioseq& default_seq,
                        CNcbiOstream& dest) const;
    void FormatFeatureInterval(const CSeq_interval& interval,
                               const CBioseq& default_seq, CNcbiOstream& dest)
        const;
    void FormatFeatureLocation(const CSeq_loc& location,
                               const CBioseq& default_seq, CNcbiOstream& dest)
        const;
    const CSeq_feat& FindFeature(const CFeat_id& id,
                                 const CBioseq& default_seq) const;

    mutable CNcbiOstream& m_Stream;
    CScope&               m_Scope;
    EFormat               m_Format;
    EVersion              m_Version;
    
    // formatting parameters; dependent on format and version
    unsigned int  m_LocusNameWidth;
    unsigned int  m_SequenceLenWidth;
    unsigned int  m_MoleculeTypeWidth;
    unsigned int  m_TopologyWidth;
    unsigned int  m_DivisionWidth;
    unsigned int  m_GenpeptBlanks;
    string        m_LengthUnit; // bases or residues
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.10  2002/12/26 12:44:39  dicuccio
* Added Win32 export specifiers
*
* Revision 1.9  2002/10/10 17:57:36  ucko
* Make most methods const (and m_Stream consequently mutable) to keep
* WorkShop from complaining when CGenbankWriter is used as a temporary.
*
* Revision 1.8  2002/05/06 16:11:12  ucko
* Merge in Andrei Gourianov's changes to use the new OM (thanks!)
* Move CVS log to end.
*
*
* *** These two entries are from src/app/id1_fetch1/genbank1.hpp ***
* Revision 1.2  2002/05/06 03:31:52  vakatov
* OM/OM1 renaming
*
* Revision 1.1  2002/04/04 16:31:36  gouriano
* id1_fetch1 - modified version of id1_fetch, which uses objmgr1
*
* Revision 1.6  2001/12/07 18:52:02  grichenk
* Updated "#include"-s and forward declarations to work with the
* new datatool version.
*
* Revision 1.5  2001/11/02 20:54:50  ucko
* Make gbqual.hpp private; clean up cruft from genbank.hpp.
*
* Revision 1.4  2001/11/01 16:32:23  ucko
* Rework qualifier handling to support appropriate reordering
*
* Revision 1.3  2001/10/12 15:34:16  ucko
* Edit in-source version of CVS log to avoid end-of-comment marker.  (Oops.)
*
* Revision 1.2  2001/10/12 15:29:04  ucko
* Drop {src,include}/objects/util/asciiseqdata.* in favor of CSeq_vector.
* Rewrite GenBank output code to take fuller advantage of the object manager.
*
* Revision 1.1  2001/09/25 20:12:04  ucko
* More cleanups from Denis.
* Put utility code in the objects namespace.
* Moved utility code to {src,include}/objects/util (to become libxobjutil).
* Moved static members of CGenbankWriter to above their first use.
* ===========================================================================
*/
#endif  /* GENBANK__HPP */
