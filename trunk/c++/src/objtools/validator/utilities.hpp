/*  $Id:
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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Definition for utility classes and functions.
 */

#ifndef VALIDATOR___UTILITIES__HPP
#define VALIDATOR___UTILITIES__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <serial/iterator.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <vector>
#include <list>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGb_qual;
class CScope;
class CSeq_entry;

BEGIN_SCOPE(validator)


// =============================================================================
//                           Enumeration of GBQual types
// =============================================================================

class CGbqualType
{
public:
    enum EType {
        e_Bad = 0,
        e_Allele,
        e_Anticodon,
        e_Bound_moiety,
        e_Citation,
        e_Clone,
        e_Codon,
        e_Codon_start,
        e_Cons_splice,
        e_Direction,
        e_EC_number,
        e_Evidence,
        e_Exception,
        e_Frequency,
        e_Function,
        e_Gene,
        e_Label,
        e_Locus_tag,
        e_Map,
        e_Mod_base,
        e_Note,
        e_Number,
        e_Organism,
        e_Partial,
        e_PCR_conditions,
        e_Phenotype,
        e_Product,
        e_Replace,
        e_Rpt_family,
        e_Rpt_type,
        e_Rpt_unit,
        e_Site,
        e_Site_type,
        e_Standard_name,
        e_Transl_except,
        e_Transl_table,
        e_Translation,
        e_Transposon,
        e_Usedin
    };

    // Conversions to enumerated type:
    static EType GetType(const string& qual);
    static EType GetType(const CGb_qual& qual);
    
    // Conversion from enumerated to string:
    static const string& GetString(EType gbqual);

private:
    CGbqualType(void);
    DECLARE_INTERNAL_ENUM_INFO(EType);
};


// =============================================================================
//                        Associating Features and GBQuals
// =============================================================================


class CFeatQualAssoc
{
public:
    typedef vector<CGbqualType::EType> TGBQualTypeVec;
    typedef map<CSeqFeatData::ESubtype, TGBQualTypeVec > TFeatQualMap;

    // Check to see is a certain gbqual is legal within the context of 
    // the specified feature
    static bool IsLegalGbqual(CSeqFeatData::ESubtype ftype,
                              CGbqualType::EType gbqual);

    // Retrieve the mandatory gbquals for a specific feature type.
    static const TGBQualTypeVec& GetMandatoryGbquals(CSeqFeatData::ESubtype ftype);

private:

    CFeatQualAssoc(void);
    void PoplulateLegalGbquals(void);
    void Associate(CSeqFeatData::ESubtype, CGbqualType::EType);
    void PopulateMandatoryGbquals(void);
    bool IsLegal(CSeqFeatData::ESubtype ftype, CGbqualType::EType gbqual);
    const TGBQualTypeVec& GetMandatoryQuals(CSeqFeatData::ESubtype ftype);
    
    static CFeatQualAssoc* Instance(void);

    static auto_ptr<CFeatQualAssoc> sm_Instance;
    // list of feature and their associated gbquals
    TFeatQualMap                    m_LegalGbquals;
    // list of features and their mandatory gbquals
    TFeatQualMap                    m_MandatoryGbquals;
};


// =============================================================================
//                                 Country Names
// =============================================================================


class CCountries
{
public:
    static bool IsValid(const string& country);

private:
    static const string sm_Countries[];
};



// =============================================================================
//                                 Functions
// =============================================================================

bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss);
bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope);
bool IsBlankString(const string& str);
bool IsBlankStringList(const list< string >& str_list);

// =============================================================================
// AnyObj:
//
// Returns true if an object of type T is embedded in object of type K,
// else false.
// =============================================================================
template <class T, class K>
bool AnyObj(const K& obj)
{
    CTypeConstIterator<T> i(ConstBegin(obj));
    return i ? true : false;
}


// ====================  Debug only -- remove before release  ==================


template<class T>
void display_object(const T& obj)
{
    auto_ptr<CObjectOStream> os (CObjectOStream::Open(eSerial_AsnText, cout));
    *os << obj;
    cout << endl;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2003/02/07 21:03:20  shomrat
* Added transposo to gbqual types
*
* Revision 1.5  2003/01/21 19:45:35  shomrat
* Added IsBlankString and IsBlankStringList
*
* Revision 1.4  2003/01/06 16:37:24  shomrat
* Add private function Associate to CFeatQualAssoc
*
* Revision 1.3  2003/01/02 21:52:43  shomrat
* Coding style changes
*
* Revision 1.2  2002/12/24 16:50:32  shomrat
* Removal of redundant GBQual types
*
* Revision 1.1  2002/12/23 20:12:42  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/


#endif  /* VALIDATOR___UTILITIES__HPP */
