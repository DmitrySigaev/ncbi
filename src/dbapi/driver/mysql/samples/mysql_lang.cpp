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
 * Author:  Anton Butanayev
 *
 * This simple program illustrates how to use the language command
 *
 */

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>

USING_NCBI_SCOPE;

int main()
{
  try
  {
    CMySQLContext my_context;
    auto_ptr<CDB_Connection> con(my_context.Connect("chopin", "test_user", "test", 0));

    {
      auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from trace.Trace limit 3"));
      lcmd->Send();
      while (lcmd->HasMoreResults())
      {
        auto_ptr<CDB_Result> r(lcmd->Result());
        for(unsigned i = 0; i < r->NofItems(); ++i)
          cout << "[" << r->ItemName(i) << "]";
        cout << endl;

        while (r->Fetch())
        {
          for(unsigned i = 0; i < r->NofItems(); ++i)
          {
            CDB_VarChar field;
            r->GetItem(&field);
            if(! field.IsNULL())
              cout << field.Value() << endl;
            else
              cout << "NULL\n";

          }
          cout << endl;
        }
      }
    }
  }
  catch (CDB_Exception& e)
  {
    CDB_UserHandler_Stream myExHandler(&cerr);
    myExHandler.HandleIt(&e);
    return 1;
  }
  return 0;
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/08/13 20:30:24  butanaev
 * Username/password changed.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
