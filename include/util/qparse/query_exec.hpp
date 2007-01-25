#ifndef UTIL__QUERY_EXEC_HPP__
#define UTIL__QUERY_EXEC_HPP__

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
 * Authors: Anatoliy Kuznetsov
 *
 * File Description: Query execution components
 *
 */

/// @file query_parse.hpp
/// Query string parsing components


#include <util/qparse/query_parse.hpp>

#include <vector>

BEGIN_NCBI_SCOPE


/** @addtogroup QParser
 *
 * @{
 */

class CQueryExec;

/// Base class for evaluation functions
///
/// Implementation guidelines for derived classes:
///    1. Make it stateless. 
///       All evaluation results should go to query node user object.
///    2. Make it reentrant
///    3. Make it thread safe and thread sync. (some functions may run in parallel)
///       (stateless class satisfies both thread safety and reenterability)
///
class NCBI_XUTIL_EXPORT CQueryFunctionBase
{
public:
    /// Vector for easy argument access
    ///
    typedef vector<CQueryParseTree::TNode*> TArgVector;

public:
    virtual ~CQueryFunctionBase();
    virtual void Evaluate(CQueryParseTree::TNode& qnode) = 0;

protected:    
    /// @name Utility functions
    /// @{
    
    /// Created vector of arguments (translate sub-nodes to vector)
    ///
    /// @param qnode 
    ///     Query node (all sub-nodes are treated as parameters
    /// @param args
    ///     Output vector
    void MakeArgVector(CQueryParseTree::TNode& qnode, 
                       TArgVector&             args);

    /// Get first sub-node
    ///                       
    CQueryParseTree::TNode* GetArg0(CQueryParseTree::TNode& qnode);
    
    /// @}

    /// Get reference on parent execution environment
    CQueryExec& GetExec() { return *m_QExec; }
    
private:    
    friend class CQueryExec;
    /// Set reference on parent query execution class
    void SetExec(CQueryExec& qexec) { m_QExec = &qexec; }
    
protected:
    CQueryExec*  m_QExec;
};


/// Query execution environment holds the function registry and the execution 
/// context. Execution context is vaguely defined term, it can be anything
/// query functions need: 
///   open files, databases, network connections, pools of threads, etc.
/// 
/// <pre>
///
/// Query execution consists of distinct several phases:
/// 1.Construction of execution environment
///    All function implementations are registered in CQueryExec
/// 2.Query parsing (see CQueryParseTree). This step transforms query (string)
///   into query tree, the tree reflects operations precedence
/// 3.Parsing tree validation and transformation (optional)
/// 4.Query execution-evaluation. Execution engine traverses the query tree calling
///   appropriate functions to do expression(query tree node) evaluation.
///   Execution results for every tree node are stored in the tree as user
///   objects (classes derived from IQueryParseUserObject).
///
/// </pre>
///
class NCBI_XUTIL_EXPORT CQueryExec
{
public:
    CQueryExec();
    virtual ~CQueryExec();
    
    /// Register function implementation. 
    /// (with ownership transfer)
    ///
    void AddFunc(CQueryParseNode::EType func_type, CQueryFunctionBase* func);
    
    /// This is a callback for implicit search nodes
    /// Our syntax allows queries like  (a=1) AND "search_term"
    /// Execution recognises "search_term" connected with logical operation 
    /// represents a special case. 
    /// This method defines a reactor (function implementation).
    ///
    void AddImplicitSearchFunc(CQueryFunctionBase* func);
    CQueryFunctionBase* GetImplicitSearchFunc() 
                    { return m_ImplicitSearchFunc.get(); }
    
    /// Return query function pointer (if registered).
    /// 
    CQueryFunctionBase* GetFunc(CQueryParseNode::EType func_type)
                                    { return m_FuncReg[(size_t)func_type]; }
    
    
    /// Run query tree evaluation
    ///
    void Evaluate(CQueryParseTree::TNode& qnode);
    
private:
    CQueryExec(const CQueryExec&);
    CQueryExec& operator=(const CQueryExec&);
    
protected:
    typedef vector<CQueryFunctionBase*> TFuncReg;
protected:
    TFuncReg                       m_FuncReg;
    auto_ptr<CQueryFunctionBase>   m_ImplicitSearchFunc;
};


/* @} */

END_NCBI_SCOPE


#endif  // UTIL__QUERY_EXEC_HPP__
