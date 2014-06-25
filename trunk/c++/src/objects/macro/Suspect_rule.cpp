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
 * Author:  J. Chen
 *
 * File Description:
 *   suspect product name check against rule
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'macro.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/macro/Suspect_rule.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSuspect_rule::~CSuspect_rule(void)
{
}

bool CSuspect_rule :: StringMatchesSuspectProductRule(const string& str) const
{
  // CSearch_func: only about string
  const CSearch_func& func = GetFind();
  if (!func.Empty() && !func.Match(str)) {
      return false;
  }
  else if (CanGetExcept()) {
     const CSearch_func& exc_func = GetExcept();
     if (!exc_func.Empty() && exc_func.Match(str)) {
       return false;
     }
  }
  return true;
};


bool CSuspect_rule::ApplyToString(string& val) const
{
    if (!IsSetReplace() || !StringMatchesSuspectProductRule(val)) {
        return false;
    }

    CRef<CString_constraint> constraint(NULL);
    if (IsSetFind() && GetFind().IsString_constraint()) {
        constraint.Reset(new CString_constraint());
        constraint->Assign(GetFind().GetString_constraint());
    }
    return GetReplace().ApplyToString(val, constraint);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1729, CRC32: 243f48c6 */
