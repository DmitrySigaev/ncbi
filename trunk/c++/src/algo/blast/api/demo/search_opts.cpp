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
 * Author:  Kevin Bealer
 *
 */

/** @file search_opts.cpp
 * Search-related options for remote_blast.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "search_opts.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

bool trace_blast_api = false;

//--------------------------------------------------------------------
//  Structures and Prototypes
//--------------------------------------------------------------------

// Functor for option reading

class COptionReader : public COptionWalker
{
public:
    COptionReader(const CArgs & args)
        : m_args(args)
    {
    }
    
    template <class ValueT, class MethodT, class OptsT>
    void Same(ValueT    & valobj,
               CUserOpt   user_opt,
               MethodT,
               CArgKey,
               COptDesc,
               OptsT    &)
    {
        ReadOpt(m_args, valobj, user_opt);
    }
    
    template <class T>
    void Local(T          & valobj,
               CUserOpt   user_opt,
               CArgKey,
               COptDesc)
    {
        ReadOpt(m_args, valobj, user_opt);
    }
    
    template <class ValueT, class MethodT, class OptsT>
    void Remote(ValueT &,
                MethodT,
                OptsT &)
    {
    }
    
    bool NeedRemote(void)
    {
        return false;
    }
    
private:
    const CArgs & m_args;
};


CNetblastSearchOpts::CNetblastSearchOpts(const CArgs & a)
{
    COptionReader optrd(a);
    Apply(optrd);
}


class CInterfaceBuilder : public COptionWalker
{
public:
    CInterfaceBuilder(CArgDescriptions & ui)
        : m_ui(ui)
    {}
    
    template <class ValueT, class MethodT, class OptsT>
    void Same(ValueT   & valobj,
              CUserOpt   user_opt,
              MethodT,
              CArgKey    arg_key,
              COptDesc   descr,
              OptsT    &)
    {
        AddOpt(m_ui, valobj, user_opt, arg_key, descr);
    }
    
    template <class T> void
    Local(T        & valobj,
          CUserOpt   user_opt,
          CArgKey    arg_key,
          COptDesc   descr)
    {
        AddOpt(m_ui, valobj, user_opt, arg_key, descr);
    }
    
    template <class ValueT, class MethodT, class OptsT>
    void Remote(ValueT &,
                MethodT,
                OptsT &)
    {
    }
    
    bool NeedRemote(void) { return false; }
    
private:
    CArgDescriptions & m_ui;
};


void CNetblastSearchOpts::x_CreateInterface2(CArgDescriptions & ui)
{
    CInterfaceBuilder make_ui(ui);
    Apply(make_ui);
}


//--------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------

void CNetblastSearchOpts::CreateInterface(CArgDescriptions & ui)
{
    CNetblastSearchOpts dummy;
    dummy.x_CreateInterface2(ui);
}

// AddOpt

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptBool         &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eBoolean);
}

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptDouble       &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eDouble);
}

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptInteger      &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eInteger);
}

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptString       &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eString);
}

// ReadOpt

void COptionWalker::ReadOpt(const CArgs & args,
                            TOptDouble  & field,
                            const char  * key)
{
    field = CheckArgsDouble(args[key]);
}

void COptionWalker::ReadOpt(const CArgs & args,
                            TOptBool    & field,
                            const char  * key)
{
    field = CheckArgsBool(args[key]);
}

void COptionWalker::ReadOpt(const CArgs & args,
                            TOptInteger & field,
                            const char  * key)
{
    field = CheckArgsInteger(args[key]);
}

void COptionWalker::ReadOpt(const CArgs & args,
                            TOptString  & field,
                            const char  * key)
{
    field = CheckArgsString(args[key]);
}

END_NCBI_SCOPE
