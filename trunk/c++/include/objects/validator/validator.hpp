#ifndef VALIDATOR___VALIDATOR__HPP
#define VALIDATOR___VALIDATOR__HPP

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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_submit;
class CSeq_annot;
class CObjectManager;
class CScope;

BEGIN_SCOPE(validator)


class NCBI_VALIDATOR_EXPORT CValidErrItem : public CObject 
{
public:
    // constructor
    CValidErrItem(EDiagSev             sev,
                  unsigned int         ei,
                  const string&        msg,
                  const CSerialObject& obj);
    // destructor
    ~CValidErrItem(void);

    // access functions
    EDiagSev                GetSeverity (void) const;
    const string&           GetSevAsStr (void) const;
    const string&           GetMsg      (void) const;
    const string&           GetErrCode  (void) const;
    const string&           GetVerbose  (void) const;
    const CConstObjectInfo& GetObject   (void) const;

private:
    // member data values
    EDiagSev         m_Severity;   // severity level
    unsigned int     m_ErrIndex;   // error code index
    string           m_Message;    // specific error message
    CConstObjectInfo m_Object;     // type plus offending object

    // internal string arrays
    static const string sm_Terse [];
    static const string sm_Verbose [];
};


class NCBI_VALIDATOR_EXPORT CValidError : public CObject
{
public:
    // constructors
    CValidError(void);
    
    void AddValidErrItem(const CValidErrItem* item);

    // Statistics
    SIZE_TYPE TotalSize(void)    const;
    SIZE_TYPE Size(EDiagSev sev) const;
    SIZE_TYPE InfoSize(void)     const;
    SIZE_TYPE WarningSize(void)  const;
    SIZE_TYPE ErrorSize(void)    const;
    SIZE_TYPE CriticalSize(void) const;
    SIZE_TYPE FatalSize(void)    const;

    // destructor
    ~CValidError(void);

private:
    typedef vector < CConstRef < CValidErrItem > > TErrs;

    // Prohibit copy constructor & assignment operator
    CValidError(const CValidError&);
    CValidError& operator= (const CValidError&);

    // Error list
    TErrs m_ErrItems;

    // statistics
    map<EDiagSev, SIZE_TYPE>     m_Stats;

    friend class CValidError_CI;
};


class NCBI_VALIDATOR_EXPORT CValidError_CI
{
public:
    CValidError_CI(void);
    CValidError_CI(const CValidError& ve,
                   const string& errcode = kEmptyStr,
                   EDiagSev minsev  = eDiag_Info,
                   EDiagSev maxsev  = eDiag_Critical);
    CValidError_CI(const CValidError_CI& iter);
    virtual ~CValidError_CI(void);

    CValidError_CI& operator=(const CValidError_CI& iter);
    CValidError_CI& operator++(void);

    operator bool(void) const;

    const CValidErrItem& operator* (void) const;
    const CValidErrItem* operator->(void) const;

private:
    bool Filter(const CValidErrItem& item) const;
    bool AtEnd(void) const;
    void Next(void);

    CConstRef<CValidError>               m_Validator;
    CValidError::TErrs::const_iterator   m_Current;

    // filters:
    string                               m_ErrCodeFilter;
    EDiagSev                             m_MinSeverity;
    EDiagSev                             m_MaxSeverity;
};


class NCBI_VALIDATOR_EXPORT CValidator : public CObject 
{
public:

    enum EValidOptions {
        eVal_non_ascii       = 1,
        eVal_no_context      = 2,
        eVal_val_align       = 4,
        eVal_val_exons       = 8,
        eVal_splice_err      = 16,
        eVal_ovl_pep_err     = 32,
        eVal_need_taxid      = 64,
        eVal_need_isojta     = 128, 
        eVal_validate_id_set = 256,

        // !!! For test purposes only {
        eVal_perf_bottlenecks  = 512
        // }
    };

    // Construtor / Destructor
    CValidator(CObjectManager& objmgr);
    ~CValidator(void);

    // Validation methods:
    // It is possible to validate objects of types CSeq_entry, CSeq_submit 
    // or CSeq_annot. In addition to the object to validate the user must 
    // provide the scope which contain that object, and validation options
    // that are created by OR'ing EValidOptions (as specified above)

    // Validate Seq-entry. 
    // If provding a scope the Seq-entry must be a 
    // top-level Seq-entry in that scope.
    auto_ptr<CValidError> Validate(const CSeq_entry& se, CScope* scope = 0,
        Uint4 options = 0);
    // Validate Seq-submit.
    // Validates each of the Seq-entry contained in the submission.
    auto_ptr<CValidError> Validate(const CSeq_submit& ss, CScope* scope = 0,
        Uint4 options = 0);
    // Validate Seq-annot
    // Validates stand alone Seq-annot objects. This will supress any
    // check on the context of the annotaions.
    auto_ptr<CValidError> Validate(const CSeq_annot& sa, CScope* scope = 0,
        Uint4 options = 0);

private:
    // Prohibit copy constructor & assignment operator
    CValidator(const CValidator&);
    CValidator& operator= (const CValidator&);

    CRef<CObjectManager>    m_ObjMgr;
};


// Inline Functions:

inline
SIZE_TYPE CValidError::TotalSize(void) const 
{
    return m_ErrItems.size();
}


inline
SIZE_TYPE CValidError::Size(EDiagSev sev) const 
{
    return const_cast<CValidError*>(this)->m_Stats[sev]; 
}


inline
SIZE_TYPE CValidError::InfoSize(void) const
{
    return Size(eDiag_Info);
}


inline
SIZE_TYPE CValidError::WarningSize(void) const
{
    return Size(eDiag_Warning);
}


inline
SIZE_TYPE CValidError::ErrorSize(void) const
{
    return Size(eDiag_Error);
}


inline
SIZE_TYPE CValidError::CriticalSize(void) const
{
    return Size(eDiag_Critical);
}


inline
SIZE_TYPE CValidError::FatalSize(void) const
{
    return Size(eDiag_Fatal);
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.9  2003/03/21 20:57:59  shomrat
* Added validate is set agins db option
*
* Revision 1.8  2003/03/20 18:52:11  shomrat
* Addes support for standalone Seq-annot validation. Decoupling the validation class (CValidator) from the error repository (CValidError)
*
* Revision 1.7  2003/03/10 18:11:53  shomrat
* Added statistics information
*
* Revision 1.6  2003/03/06 19:31:57  shomrat
* Bug fix and code cleanup in CValidError_CI
*
* Revision 1.5  2003/02/24 20:14:59  shomrat
* Added AddValidErrItem instead of exposing the undelying container
*
* Revision 1.4  2003/02/07 21:00:27  shomrat
* Added size()
*
* Revision 1.3  2003/02/03 20:16:55  shomrat
* Added option to supress suspected performance bottlenecks (for testing)
*
* Revision 1.2  2003/01/07 19:57:03  shomrat
* Added Win32 export specifier; Memebr GetMessage() changed to GetMsg() due to conflict
*
* Revision 1.1  2002/12/19 20:54:23  shomrat
* From /objects/util/validate.hpp
*
*
* ===========================================================================
*/

#endif  /* VALIDATOR___VALIDATOR__HPP */
