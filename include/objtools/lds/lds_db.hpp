#ifndef LDS_DB_HPP__
#define LDS_DB_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:   Local data storage, database description.
 *
 */
#include <bdb/bdb_file.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



struct NCBI_LDS_EXPORT SLDS_FileDB : public CBDB_File
{
    CBDB_FieldInt4     file_id;
    CBDB_FieldString   file_name;
    CBDB_FieldInt4     format;
    CBDB_FieldInt4     time_stamp;
    CBDB_FieldInt4     CRC;
    CBDB_FieldInt4     file_size;   

    SLDS_FileDB();
};



struct NCBI_LDS_EXPORT SLDS_AnnotDB : public CBDB_File
{
    CBDB_FieldInt4    annot_id;
    CBDB_FieldInt4    file_id;
    CBDB_FieldInt4    annot_type;
    CBDB_FieldInt4    file_offset;

    SLDS_AnnotDB();
};


struct NCBI_LDS_EXPORT SLDS_ObjectTypeDB : public CBDB_File
{
    CBDB_FieldInt4    object_type;
    CBDB_FieldString  type_name;

    SLDS_ObjectTypeDB();
};


struct NCBI_LDS_EXPORT SLDS_ObjectDB : public CBDB_File
{
    CBDB_FieldInt4    object_id;

    CBDB_FieldInt4    file_id; 
    CBDB_FieldString  primary_seqid;
    CBDB_FieldInt4    seqlist_id; 
    CBDB_FieldInt4    object_type;
    CBDB_FieldInt4    file_offset;
    CBDB_FieldInt4    object_attr_id;
    CBDB_FieldInt4    TSE_object_id;     // TOP level seq entry object id
    CBDB_FieldInt4    parent_object_id;  // Parent SeqEntry object id


    SLDS_ObjectDB();
};


struct NCBI_LDS_EXPORT SLDS_Annot2ObjectDB : public CBDB_File
{
    CBDB_FieldInt4  object_id;
    CBDB_FieldInt4  annot_id;

    SLDS_Annot2ObjectDB();
};


struct NCBI_LDS_EXPORT SLDS_ObjectAttrDB : public CBDB_File
{
    CBDB_FieldInt4    object_attr_id;

    CBDB_FieldString  object_title;
    CBDB_FieldString  organism;
    CBDB_FieldString  keywords;
    CBDB_FieldString  seq_ids;

    SLDS_ObjectAttrDB();
};


// Structure puts together all tables used in LDS
struct NCBI_LDS_EXPORT SLDS_TablesCollection
{
    SLDS_FileDB          file_db;
    SLDS_ObjectTypeDB    object_type_db;
    SLDS_ObjectDB        object_db;
    SLDS_ObjectAttrDB    object_attr_db;
    SLDS_AnnotDB         annot_db;
    SLDS_Annot2ObjectDB  annot2obj_db;
};


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



inline SLDS_FileDB::SLDS_FileDB()
{
    BindKey("file_id", &file_id);

    BindData("file_name", &file_name, _MAX_PATH * 2);
    BindData("format", &format);
    BindData("time_stamp", &time_stamp, 64);
    BindData("CRC", &CRC);
    BindData("file_size", &file_size);
}


inline SLDS_AnnotDB::SLDS_AnnotDB()
{
    BindKey("annot_id", &annot_id);

    BindData("file_id", &file_id);
    BindData("annot_type", &annot_type);
    BindData("file_offset", &file_offset);
}


inline SLDS_ObjectTypeDB::SLDS_ObjectTypeDB()
{
    BindKey("object_type", &object_type);

    BindData("type_name",  &type_name);
}


inline SLDS_ObjectDB::SLDS_ObjectDB()
{
    BindKey("object_id", &object_id);

    BindData("file_id", &file_id);
    BindData("primary_seqid", &primary_seqid);
    BindData("seqlist_id", &seqlist_id);
    BindData("object_type", &object_type);
    BindData("file_offset", &file_offset);
    BindData("object_attr_id", &object_attr_id);
    BindData("TSE_object_id", &TSE_object_id);
    BindData("parent_object_id", &parent_object_id);
}


inline SLDS_ObjectAttrDB::SLDS_ObjectAttrDB()
{
    BindKey("object_attr_id", &object_attr_id);

    BindData("object_title", &object_title, 512);
    BindData("organism", &organism, 256);
    BindData("keywords", &keywords, 1024);
    BindData("seq_ids", &seq_ids, 65536);
}



inline SLDS_Annot2ObjectDB::SLDS_Annot2ObjectDB()
{
    BindKey("object_id", &object_id);
    BindKey("annot_id", &annot_id);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.9  2003/06/13 15:59:05  kuznets
* Added space separated list of all sequence ids (object attributes)
*
* Revision 1.8  2003/06/04 16:33:32  kuznets
* Increased length of object_title field in SLDS_ObjectAttrDB
*
* Revision 1.7  2003/06/03 19:14:02  kuznets
* Added lds dll export/import specifications
*
* Revision 1.6  2003/05/30 20:26:33  kuznets
* Fixed field binding in "objects"
*
* Revision 1.5  2003/05/30 14:05:39  kuznets
* Added primary_seqid field to the objects table.
*
* Revision 1.4  2003/05/23 20:33:33  kuznets
* Bulk changes in lds library, code reorganizations, implemented top level
* objects read, metainformation persistance implemented for top level objects...
*
* Revision 1.3  2003/05/23 18:21:21  kuznets
* +SLDS_TablesCollection
*
* Revision 1.2  2003/05/22 19:11:35  kuznets
* +SLDS_ObjectTypeDB
*
* Revision 1.1  2003/05/22 13:24:45  kuznets
* Initial revision
*
*
* ===========================================================================
*/

#endif

