#ifndef NCBIAPP__HPP
#define NCBIAPP__HPP

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
* Author: 
*	Vsevolod Sandomirskiy
*
* File Description:
*   Basic Application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/11/02 22:10:12  sandomir
* CNcbiApplication added; netest sample updated
*
* ===========================================================================
*/

#include <vector>
#include <string>

//
// class CNcbiApplication
//

/*
 This is base application class. User's code has to be roughly like this:

 int main(  int argc, char* argv[] )
{
   CMyApplication app( argc, argv );
   try {
      app.Init(); 
      int res = app.Run(); 
      app.Exit(); 
   } catch(...) {}
*/

class CNcbiApplication
{
public:

  CNcbiApplication( int argc, char* argv[] ) throw();
  virtual ~CNcbiApplication( void ) throw();

  virtual void Init( void ); // initialization
  virtual void Exit( void ); // cleanup

  virtual int Run( void ); 

protected:

  // saved command line parameters
  std::vector< std::string, std::allocator<std::string> > m_argv;

};


#endif // NCBIAPP__HPP
