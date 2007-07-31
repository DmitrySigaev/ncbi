#ifndef CORELIB___NCBI_CONFIG__HPP
#define CORELIB___NCBI_CONFIG__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 */

/// @file ncbi_config.hpp
/// Parameters initialization model

#include <corelib/ncbi_tree.hpp>
#include <corelib/ncbimisc.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup ModuleConfig
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
/// CConfigException --
///
/// Exception generated by configutation API

class NCBI_XNCBI_EXPORT CConfigException : public CCoreException
{
public:
    enum EErrCode {
        eParameterMissing,      ///< Missing mandatory parameter
        eSynonymDuplicate
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CConfigException, CCoreException);
};



class IRegistry;

class NCBI_XNCBI_EXPORT CConfig
{
public:
    /// Instantiation parameters tree.
    /// 
    /// Plugin manager's intantiation model is based on class factories.
    /// Recursive class factory calls are modeled as tree, where specific
    /// subtree is responsible for CF parameters
    ///
    typedef CTreePair<string, string>  TParamValue;
    typedef TParamValue::TPairTreeNode TParamTree;

public:
    /// Optionally takes ownership on passed param_tree
    CConfig(TParamTree* param_tree, EOwnership own = eTakeOwnership);

    /// Construct, take no tree ownership
    CConfig(const TParamTree* param_tree);

    /// Take registry and create a config tree out of it
    CConfig(const IRegistry& reg);
    ~CConfig();

    /// Defines how to behave when parameter is missing
    enum EErrAction {
        eErr_Throw,    ///< Throw an exception on error
        eErr_NoThrow   ///< Return default value on error
    };

    /// Utility function to get an element of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    ///
    /// @param driver_name
    ///    Name of the modure requesting parameter (used in diagnostics)
    /// @param params
    ///    Parameters tree
    /// @param param_name
    ///    Name of the parameter
    /// @param mandatory
    ///    Error action
    /// @param default_value
    ///    Default value for missing parameters
    string GetString(const string&  driver_name,
                     const string&  param_name, 
                     EErrAction     on_error,
                     const string&  default_value,
                     const list<string>* synonyms = NULL);

    /// This version always defaults to the empty string so that it
    /// can safely return a reference.  (default_value may be
    /// temporary in some cases.)
    const string& GetString(const string&  driver_name,
                            const string&  param_name, 
                            EErrAction     on_error,
                            const list<string>* synonyms = NULL);
    
    /// Utility function to get an integer element of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    ///
    /// @param driver_name
    ///    Name of the modure requesting parameter (used in diagnostics)
    /// @param params
    ///    Parameters tree
    /// @param param_name
    ///    Name of the parameter
    /// @param mandatory
    ///    Error action
    /// @param default_value
    ///    Default value for missing parameters
    /// @sa ParamTree_GetString
    int GetInt(const string&  driver_name,
               const string&  param_name, 
               EErrAction     on_error,
               int            default_value,
               const list<string>* synonyms = NULL);

    /// Utility function to get an integer element of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    /// This function undestands KB, MB, GB qualifiers at the end of the string
    ///
    /// @param driver_name
    ///    Name of the modure requesting parameter (used in diagnostics)
    /// @param params
    ///    Parameters tree
    /// @param param_name
    ///    Name of the parameter
    /// @param mandatory
    ///    Error action
    /// @param default_value
    ///    Default value for missing parameters
    /// @sa ParamTree_GetString
    Uint8 GetDataSize(const string&  driver_name,
                      const string&  param_name, 
                      EErrAction     on_error,
                      unsigned int   default_value,
                      const list<string>* synonyms = NULL);

    /// Utility function to get an integer element of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    ///
    /// @param driver_name
    ///    Name of the modure requesting parameter (used in diagnostics)
    /// @param params
    ///    Parameters tree
    /// @param param_name
    ///    Name of the parameter
    /// @param mandatory
    ///    Error action
    /// @param default_value
    ///    Default value for missing parameters
    /// @sa ParamTree_GetString
    bool GetBool(const string&  driver_name,
                 const string&  param_name, 
                 EErrAction     on_error,
                 bool           default_value,
                 const list<string>* synonyms = NULL);

    const TParamTree* GetTree() const { return m_ParamTree.get(); }

    /// Reconstruct param tree from the application registry
    /// @param reg
    ///     Application registry (loaded from the INI file)
    /// @return 
    ///     Reconstructed tree (caller is responsible for deletion)
    static TParamTree* ConvertRegToTree(const IRegistry&  reg);

private:
    CConfig(const CConfig&);
    CConfig& operator=(const CConfig&);

protected:
    AutoPtr<TParamTree> m_ParamTree;
};

/* @} */


END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_CONFIG__HPP */
