#ifndef TEST_MT__HPP
#define TEST_MT__HPP

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
 * Author:  Aleksey Grichenko
 *
 *
 */

/// @file test_mt.hpp
///   Wrapper for testing modules in MT environment


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>


/** @addtogroup MTWrappers
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// Globals

/// Minimum number of threads.
const unsigned int k_NumThreadsMin = 1;

/// Maximum number of threads.
const unsigned int k_NumThreadsMax = 500;

/// Minimum number of spawn by threads.
const int k_SpawnByMin = 1;

/// Maximum number of spawn by threads.
const int k_SpawnByMax = 100;

extern unsigned int  s_NumThreads;
extern int           s_SpawnBy;


/////////////////////////////////////////////////////////////////////////////
//  Test application
//
//    Core application class for MT-tests
//



/////////////////////////////////////////////////////////////////////////////
///
/// CThreadedApp --
///
/// Basic NCBI threaded application class.
///
/// Defines the high level behavior of an NCBI threaded application.

class NCBI_XNCBI_EXPORT CThreadedApp : public CNcbiApplication
{
public:
    /// Constructor.
    ///
    /// Override constructor to initialize the application.
    CThreadedApp(void);

    /// Destructor.
    ~CThreadedApp(void);

    // Functions to be called by the test thread's constructor, Main(),
    // OnExit() and destructor. All methods should return "true" on
    // success, "false" on failure.

    /// Initialize the thread.
    ///
    /// Called from thread's constructor.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Init(int idx);

    /// Run the thread.
    ///
    /// Called from thread's Main() method.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Run(int idx);

    /// Exit the thread.
    ///
    /// Called from thread's OnExit() method.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Exit(int idx);

    /// Destroy the thread.
    ///
    /// Called from thread's destroy() method.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool Thread_Destroy(int idx);

protected:

    /// Override this method to add your custom arguments.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool TestApp_Args( CArgDescriptions& args);

    /// Override this method to execute code before running threads.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool TestApp_Init(void);

    /// Override this method to execute code after all threads terminate.
    /// @return
    ///   TRUE on success, and FALSE on failure.
    virtual bool TestApp_Exit(void);

private:

    /// Initialize the thread.
    void Init(void);

    /// Run the thread.
    int Run(void);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/09/08 12:18:13  siyan
 * Documentation changes
 *
 * Revision 1.5  2003/05/08 20:50:08  grichenk
 * Allow MT tests to run in ST mode using CThread::fRunAllowST flag.
 *
 * Revision 1.4  2003/04/01 19:19:44  siyan
 * Added doxygen support
 *
 * Revision 1.3  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.2  2002/04/30 19:10:26  gouriano
 * added possibility to add custom arguments
 *
 * Revision 1.1  2002/04/23 13:12:28  gouriano
 * test_mt.hpp moved into another location
 *
 * Revision 6.5  2002/04/16 18:49:07  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.4  2002/04/11 20:00:46  ivanov
 * Returned standard assert() vice CORE_ASSERT()
 *
 * Revision 6.3  2002/04/10 18:38:51  ivanov
 * Moved CVS log to end of file. Changed assert() to CORE_ASSERT()
 *
 * Revision 6.2  2001/05/17 15:05:08  lavr
 * Typos corrected
 *
 * Revision 6.1  2001/04/06 15:53:08  grichenk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* TEST_MT__HPP */
