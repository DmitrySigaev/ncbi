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
* Authors:  Paul Thiessen
*
* File Description:
*      test suite for asn serialization library, asn data i/o
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <util/random_gen.hpp>

#include <memory>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Global_id.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/mmdb1/Atom.hpp>
#include <objects/mmdb1/Atom_id.hpp>
#include <objects/mmdb2/Model_space_points.hpp>
#include <objects/mmdb3/Region_coordinates.hpp>
#include <objects/cn3d/Cn3d_color.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/id1_client.hpp>

#include "asniotest.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// diagnostic streams
#define INFOMSG(stream) ERR_POST(Info << stream)
#define WARNINGMSG(stream) ERR_POST(Warning << stream)
#define ERRORMSG(stream) ERR_POST(Error << stream << '!')

// directory that contains test files (default overridden by env. var. ASNIOTEST_FILES)
string pathToFiles(".");

// keep track of files created, to remove them at the end
list < string > filesCreated;

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
bool ReadASNFromFile(const char *filename, ASNClass *ASNobject, bool isBinary, string *err)
{
    err->erase();

    // initialize the binary input stream
    auto_ptr<CNcbiIstream> inStream;
    inStream.reset(new CNcbiIfstream(
        (pathToFiles + CDirEntry::GetPathSeparator() + filename).c_str(),
        IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
        *err = "Cannot open file for reading";
        return false;
    }

    auto_ptr<CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new CObjectIStreamAsnBinary(*inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new CObjectIStreamAsn(*inStream));
    }

    // Read the asn data
    bool okay = true;
    try {
        *inObject >> *ASNobject;
        INFOMSG("read file " << filename);
    } catch (exception& e) {
        *err = e.what();
        okay = false;
    }

    return okay;
}

// for writing out ASN data (to temporary files); all files written this way will be removed at program end
template < class ASNClass >
bool WriteASNToFile(const ASNClass& ASNobject, bool isBinary,
    string *err, EFixNonPrint fixNonPrint = eFNP_Default)
{
    err->erase();

    // construct a temp file name
    CNcbiOstrstream oss;
    oss << "test_" << (filesCreated.size() + 1) << (isBinary ? ".bin" : ".txt");
    string filename = CNcbiOstrstreamToString(oss);
    string fullPath = CDirEntry::MakePath(pathToFiles, filename);
    if (CFile(fullPath).Exists()) {
        WARNINGMSG("Overwriting temporary file " << fullPath);
        // *err = "Can't overwrite file ";
        // *err += fullPath;
        // return false;
    }

    // initialize a binary output stream
    auto_ptr<CNcbiOstream> outStream;
    outStream.reset(new CNcbiOfstream(
        fullPath.c_str(),
        isBinary ? (IOS_BASE::out | IOS_BASE::binary) : IOS_BASE::out));
    if (!(*outStream)) {
        *err = "Cannot open file for writing";
        return false;
    }
    filesCreated.push_back(fullPath);

    auto_ptr<CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new CObjectOStreamAsnBinary(*outStream, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new CObjectOStreamAsn(*outStream, fixNonPrint));
    }

    // write the asn data
    bool okay = true;
    try {
        *outObject << ASNobject;
        outStream->flush();
        INFOMSG("wrote file " << filename);
    } catch (exception& e) {
        *err = e.what();
        okay = false;
    }

    return okay;
}

template < class ASNClass >
bool WriteASNToFilesTxtBin(const ASNClass& ASNobject, string *err)
{
    return (WriteASNToFile(ASNobject, false, err) && WriteASNToFile(ASNobject, true, err));
}

// test function macros
#define BEGIN_TEST_FUNCTION(func) \
    int func (void) { \
        int nErrors = 0; \
        string err; \
        INFOMSG("Running " #func " test");

#define END_TEST_FUNCTION \
        return nErrors; \
    }

#define ADD_ERR(msg) \
    do { \
        ERRORMSG(msg); \
        nErrors++; \
    } while (0)

#define ADD_ERR_RETURN(msg) \
    do { \
        ERRORMSG(msg); \
        nErrors++; \
        return nErrors; \
    } while (0)

#define SHOULD_THROW_EXCEPTION(object, accessor) \
    do { \
        try { \
            object.accessor(); \
            ADD_ERR(#accessor "() should have thrown an exception"); \
        } catch (exception& e) { \
            INFOMSG(#accessor "() correctly threw exception " << e.what()); \
        } \
    } while (0)


// tests reading and writing of asn data files
BEGIN_TEST_FUNCTION(BasicFileIO)

    // read in a trivial object
    CSeq_id seqId;
    if (!ReadASNFromFile("pdbSeqId.txt", &seqId, false, &err))
        ADD_ERR_RETURN("failed to load pdbSeqId.txt: " << err);

    // test text and binary output
    if (!WriteASNToFilesTxtBin(seqId, &err))
        ADD_ERR("failed to write CSeq_id: " << err);

    // now try a bigger object (in binary this time)
    CCdd cdd;
    if (!ReadASNFromFile("ADF.bin", &cdd, true, &err))
        ADD_ERR_RETURN("failed to load ADF.bin: " << err);
    if (!WriteASNToFilesTxtBin(cdd, &err))
        ADD_ERR("failed to write CCdd: " << err);

    // structure
    CNcbi_mime_asn1 mime;
    if (!ReadASNFromFile("1doi.bin", &mime, true, &err))
        ADD_ERR_RETURN("failed to load 1doi.bin: " << err);
    if (!WriteASNToFilesTxtBin(mime, &err))
        ADD_ERR("failed to write CNcbi_mime_asn1: " << err);

END_TEST_FUNCTION


// tests output of Assign()-copied objects
BEGIN_TEST_FUNCTION(AssignAndOutput)

    // try to output a copied simple object
    CSeq_id s1;
    if (!ReadASNFromFile("pdbSeqId.txt", &s1, false, &err))
        ADD_ERR_RETURN("failed to load pdbSeqId.txt: " << err);
    CSeq_id s2;
    s2.Assign(s1);
    if (!WriteASNToFilesTxtBin(s2, &err))
        ADD_ERR("failed to write Assign()'ed Seq-id: " << err);

    // try to output a copied complex object
    CCdd c1;
    if (!ReadASNFromFile("ADF.bin", &c1, true, &err))
        ADD_ERR_RETURN("failed to load ADF.bin: " << err);
    CCdd c2;
    c2.Assign(c1);
    if (!WriteASNToFilesTxtBin(c2, &err))
        ADD_ERR("failed to write Assign()'ed Cdd: " << err);

    // structure
    CNcbi_mime_asn1 m1;
    if (!ReadASNFromFile("1doi.bin", &m1, true, &err))
        ADD_ERR_RETURN("failed to load 1doi.bin: " << err);
    CNcbi_mime_asn1 m2;
    m2.Assign(m1);
    if (!WriteASNToFilesTxtBin(m2, &err))
        ADD_ERR("failed to write Assign()'ed Ncbi_mime_asn1: " << err);

END_TEST_FUNCTION


// tests operations and validations of a mandatory asn field (id and element from Atom)
BEGIN_TEST_FUNCTION(MandatoryField)

    // read in a good asn blob
    CAtom a1;
    if (!ReadASNFromFile("goodAtom.txt", &a1, false, &err))
        ADD_ERR_RETURN("failed to load goodAtom.txt: " << err);

    // try to read in a bad asn blob, missing a field - should fail
    INFOMSG("reading badAtom.txt - should fail");
    CAtom a2;
    if (ReadASNFromFile("badAtom.txt", &a2, false, &err))
        ADD_ERR_RETURN("badAtom.txt should not have loaded successfully");

    // Get/CanGet/Set/IsSet tests on loaded and created objects
    if (!a1.CanGetId() || !a1.IsSetId() || a1.GetId() != 37)
        ADD_ERR("id access failed");
    CAtom a3;
    if (a3.CanGetElement() || a3.IsSetElement())
        ADD_ERR("new Atom should not have element");
    SHOULD_THROW_EXCEPTION(a3, GetElement);
    a3.SetElement(CAtom::eElement_lr);
    if (!a3.CanGetElement() || !a3.IsSetElement() || a3.GetElement() != CAtom::eElement_lr)
        ADD_ERR("bad element state after SetElement()");

    // shouldn't be able to write without id
    INFOMSG("trying to write incomplete Atom - should fail");
    if (WriteASNToFilesTxtBin(a3, &err))
        ADD_ERR("should not be able to write Atom with no id");
    a3.SetId().Set(92);
    if (!WriteASNToFilesTxtBin(a3, &err))
        ADD_ERR("failed to write complete Atom: " << err);

END_TEST_FUNCTION


// test list (SEQUENCE) field
BEGIN_TEST_FUNCTION(ListField)

    // read in a valid object, with a mandatory but empty list
    CModel_space_points m1;
    if (!ReadASNFromFile("goodMSP.txt", &m1, false, &err))
        ADD_ERR_RETURN("failed to read goodMSP.txt: " << err);
    if (!m1.IsSetX() || !m1.CanGetX() || m1.GetX().size() != 3 || m1.GetX().front() != 167831)
        ADD_ERR("x access failed");
    if (!m1.IsSetZ() || !m1.CanGetZ() || m1.GetZ().size() != 0)
        ADD_ERR("z access failed");

    // test created object
    CModel_space_points m2;
    if (m2.IsSetX() || !m2.CanGetX() || m2.GetX().size() != 0)
        ADD_ERR_RETURN("bad x state in new object");
    m2.SetScale_factor() = 15;
    m2.SetX().push_back(21);
    m2.SetX().push_back(931);
    if (!m2.IsSetX() || !m2.CanGetX() || m2.GetX().size() != 2)
        ADD_ERR_RETURN("bad x state after SetX()");
    m2.SetY().push_back(0);
    INFOMSG("trying to write incomplete Model-space-points - should fail");
    if (WriteASNToFilesTxtBin(m2, &err))
        ADD_ERR("shouldn't be able to write incomplete Model-space-points");
    m2.SetZ();  // set but empty!
    if (!WriteASNToFilesTxtBin(m2, &err))
        ADD_ERR("failed to write complete Model-space-points: " << err);

    // missing a mandatory list
    CModel_space_points m3;
    INFOMSG("trying to read incomplete Model-space-points - should fail");
    if (ReadASNFromFile("badMSP.txt", &m3, false, &err))
        ADD_ERR("shouldn't be able to read incomplete Model-space-points: " << err);

    // OPTIONAL list (coordinate-indices from Region-coordinates)
    CRegion_coordinates r1;
    if (!ReadASNFromFile("goodRC.txt", &r1, false, &err))
        ADD_ERR_RETURN("failed to read goodRC.txt: " << err);
    if (r1.IsSetCoordinate_indices() || !r1.CanGetCoordinate_indices() ||
            r1.GetCoordinate_indices().size() != 0)
        ADD_ERR("bad state for missing OPTIONAL list");

END_TEST_FUNCTION


// test OPTIONAL fields (using Global-id type)
BEGIN_TEST_FUNCTION(OptionalField)

    // object read from file
    CGlobal_id g1;
    if (!ReadASNFromFile("goodGID.txt", &g1, false, &err))
        ADD_ERR_RETURN("failed to read goodGID.txt: " << err);
    if (g1.IsSetRelease() || g1.CanGetRelease())
        ADD_ERR("bad release state");
    SHOULD_THROW_EXCEPTION(g1, GetRelease);
    if (!g1.IsSetVersion() || !g1.CanGetVersion() || g1.GetVersion() != 37)
        ADD_ERR("bad version state");

    // new object
    CGlobal_id g2;
    if (g2.IsSetRelease() || g2.CanGetRelease())
        ADD_ERR("bad release state in new object");
    SHOULD_THROW_EXCEPTION(g2, GetRelease);
    g2.SetAccession("something");
    g2.SetRelease("something else");
    if (!g2.IsSetRelease() || !g2.CanGetRelease())
        ADD_ERR("bad release state after SetRelease()");
    if (!WriteASNToFilesTxtBin(g2, &err))
        ADD_ERR("failed to write good Global-id: " << err);

END_TEST_FUNCTION


// check fields with DEFAULT value
BEGIN_TEST_FUNCTION(DefaultField)

    CCn3d_color c1;
    if (!ReadASNFromFile("goodColor.txt", &c1, false, &err))
        ADD_ERR_RETURN("failed to read goodColor.txt: " << err);
    if (!c1.IsSetScale_factor() || !c1.CanGetScale_factor() || c1.GetScale_factor() != 17)
        ADD_ERR("bad scale-factor access");
    if (c1.IsSetAlpha() || !c1.CanGetAlpha() || c1.GetAlpha() != 255)
        ADD_ERR("bad alpha access");

    CCn3d_color c2;
    if (c2.IsSetAlpha() || !c2.CanGetAlpha() || c2.GetAlpha() != 255)
        ADD_ERR("bad alpha state after construction");
    c2.SetAlpha(37);
    if (!c2.IsSetAlpha() || !c2.CanGetAlpha() || c2.GetAlpha() != 37)
        ADD_ERR("bad alpha state after SetAlpha()");
    c2.SetRed(1);
    c2.SetGreen(2);
    c2.SetBlue(3);
    if (!WriteASNToFilesTxtBin(c2, &err))
        ADD_ERR("failed to write good Cn3d-color: " << err);

END_TEST_FUNCTION

// check problem with zero Real data
BEGIN_TEST_FUNCTION(ZeroReal)

    CUser_object originalUO, newUO;
    if(!ReadASNFromFile("userObject.txt", &originalUO, false, &err))
        ADD_ERR_RETURN("failed to read userObject.txt: " << err);
    if (!WriteASNToFilesTxtBin(originalUO, &err))
        ADD_ERR("failed to write original User-object: " << err);

    // copy via streams
    CNcbiStrstream asnIOstream;
    ncbi::CObjectOStreamAsnBinary outObject(asnIOstream);
    outObject << originalUO;
    ncbi::CObjectIStreamAsnBinary inObject(asnIOstream);
    inObject >> newUO;

    if (!WriteASNToFilesTxtBin(newUO, &err))
        ADD_ERR("failed to write new User-object: " << err);

END_TEST_FUNCTION


BEGIN_TEST_FUNCTION(FullBlobs)
    CNcbiIfstream gilist
        (CDirEntry::MakePath(pathToFiles, "representativeGIs.txt").c_str());
    string        line;
    CID1Client    id1;
    CRandom       rng(CurrentTime().GetTimeT());
    id1.SetAllowDeadEntries(true);
    while (NcbiGetlineEOL(gilist, line)) {
        if (line.empty()  ||  !isdigit(line[0]) ) {
            continue;
        }
        int gi = NStr::StringToInt(line, 10, NStr::eCheck_Skip);
        if (gi <= 0) {
            continue;
        }
        INFOMSG("Trying GI " << gi);
        CRef<CSeq_entry> se;
        try {
            CID1server_maxcomplex req;
            req.SetGi(gi);
            // set bits in maxplex to exclude external annotations
            req.SetMaxplex(eEntry_complexities_entry | (0xffff << 4));
            se = id1.AskGetsefromgi(req);
        } catch (exception& e) {
            ADD_ERR("failed to retrieve blob for GI " << gi << ": "
                    << e.what());
            continue;
        }
        ESerialDataFormat sdf = eSerial_None;
        string            sdf_name;
        switch (rng.GetRand(0, 2)) {
        case 0:  sdf = eSerial_AsnText;    sdf_name = "text ASN.1";    break;
        case 1:  sdf = eSerial_AsnBinary;  sdf_name = "binary ASN.1";  break;
        case 2:  sdf = eSerial_Xml;        sdf_name = "XML";           break;
        }
        CConn_MemoryStream stream;
        try {
            auto_ptr<CObjectOStream> out(CObjectOStream::Open(sdf, stream));
            *out << *se;
        } catch (exception& e) {
            ADD_ERR("failed to generate " << sdf_name << " for GI " << gi
                    << ": " << e.what());
            continue;
        }
        CSeq_entry se2;
        try {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(sdf, stream));
            *in >> se2;
        } catch (exception& e) {
            ADD_ERR("failed to parse " << sdf_name << " for GI " << gi << ": "
                    << e.what());
            continue;
        }
        if (!SerialEquals(*se, se2)) {
            ADD_ERR("failed equality after " << sdf_name
                    << " round trip for GI " << gi);
        }
    }
END_TEST_FUNCTION


// test to make sure that a value of -1 converted to an unsigned int will still read/write
BEGIN_TEST_FUNCTION(UnsignedInt)

    int signedInt = -1;
    CRef < CSeq_loc > seqloc(new CSeq_loc());
    seqloc->SetInt().SetFrom(signedInt);
    seqloc->SetInt().SetTo(signedInt);
    seqloc->SetInt().SetId().SetGi(0);

    if (!WriteASNToFile(*seqloc, false, &err))
        ADD_ERR_RETURN("output of seqloc with '-1' failed: " << err);

    // load latest test output file
    CNcbiOstrstream oss;
    oss << "test_" << filesCreated.size() << ".txt";
    string filename = CNcbiOstrstreamToString(oss);

    CRef < CSeq_loc > seqloc2(new CSeq_loc());
    if (!ReadASNFromFile(filename.c_str(), seqloc2.GetPointer(), false, &err))
        ADD_ERR_RETURN("input of seqloc with '-1' failed: " << err);
    if (!seqloc2->Equals(*seqloc))
        ADD_ERR("seqloc with '-1' Equals test failed");

    CRef < CSeq_loc > seqloc3(new CSeq_loc());
    seqloc3->Assign(*seqloc);
    if (!WriteASNToFile(*seqloc3, false, &err))
        ADD_ERR("output of Assign'ed seqloc with '-1' failed: " << err);
    if (((int) seqloc3->GetInt().GetFrom()) != -1)
        ADD_ERR("seqloc post-Assign value test failed");

    CRef < CSeq_loc > seqloc4(new CSeq_loc());
    if (!ReadASNFromFile("seqLocTest.txt", seqloc4.GetPointer(), false, &err))
        ADD_ERR_RETURN("reading seqLocTest.txt failed: " << err);
    if (seqloc4->GetInt().GetFrom() != ((unsigned int) 4294967295))
        ADD_ERR("seqloc unsigned int value test failed");

END_TEST_FUNCTION


// to call test functions, counting errors
#define RUN_TEST(func) \
    do { \
        int errors = func(); \
        nErrors += errors; \
        if (errors) \
            ERRORMSG(#func " test failed"); \
    } while (0)

int ASNIOTestApp::Run(void)
{
    if (GetEnvironment().Get("ASNIOTEST_FILES").size() > 0) {
        pathToFiles = GetEnvironment().Get("ASNIOTEST_FILES");
    } else if (CDirEntry("data/pdbSeqId.txt").Exists()) {
        pathToFiles = "data";
    }
    INFOMSG("Looking for sample files in " << pathToFiles);

    int nErrors = 0;
    try {
        INFOMSG("Running tests...");

        // individual tests
        RUN_TEST(BasicFileIO);
        RUN_TEST(AssignAndOutput);
        RUN_TEST(MandatoryField);
        RUN_TEST(ListField);
        RUN_TEST(OptionalField);
        RUN_TEST(DefaultField);
        RUN_TEST(ZeroReal);
        RUN_TEST(FullBlobs);
        RUN_TEST(UnsignedInt);

    } catch (exception& e) {
        ERRORMSG("uncaught exception: " << e.what());
        nErrors++;
    }

    while (filesCreated.size() > 0) {
        CFile file(filesCreated.front());
        if (!file.Remove()) {
            ERRORMSG("Couldn't remove file " << filesCreated.front());
            nErrors++;
        }
        filesCreated.pop_front();
    }

    if (nErrors == 0)
        INFOMSG("No errors encountered, all tests succeeded!");
    else
        ERRORMSG(nErrors << " error" << ((nErrors == 1) ? "" : "s") << " encountered");
#ifdef _DEBUG
    INFOMSG("(Debug mode build, " << __DATE__ << ")");
#else
    INFOMSG("(Release mode build, " << __DATE__ << ")");
#endif
    return nErrors;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // diagnostic streams setup
    SetDiagStream(&NcbiCout);       // send all diagnostic messages to cout
    SetDiagPostLevel(eDiag_Info);   // show all messages

    // turn on C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    ASNIOTestApp app;
    return app.AppMain(argc, argv, NULL, eDS_Default, NULL);    // don't use config file
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2004/09/10 16:44:53  vasilche
* Exclude external annotations from blobs loaded from ID1.
*
* Revision 1.15  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.14  2004/03/09 21:19:53  ucko
* Only warn about overwriting temporary files, since the previous run
* may have crashed and left them behind.
*
* Revision 1.13  2004/02/12 18:33:29  thiessen
* add Assign test to UnsignedInt test
*
* Revision 1.12  2004/02/12 17:59:58  thiessen
* remove one gcc warning
*
* Revision 1.11  2004/02/12 17:55:12  thiessen
* add UnsignedInt test
*
* Revision 1.10  2004/02/02 22:12:31  ucko
* Test retrieval and round-trip conversion of a variety of representative blobs.
*
* Revision 1.9  2004/01/23 16:29:52  ucko
* Try to find test files in data/.
*
* Revision 1.8  2004/01/15 14:35:13  thiessen
* add date to output
*
* Revision 1.7  2004/01/12 22:43:08  thiessen
* add ZeroReal test
*
* Revision 1.6  2003/12/11 17:52:10  thiessen
* add Optional and Default field tests
*
* Revision 1.5  2003/12/11 15:21:02  thiessen
* add mandatory and list tests
*
* Revision 1.4  2003/11/25 20:53:02  ucko
* Make template-accessed variables non-static to fix compilation on WorkShop.
*
* Revision 1.3  2003/11/25 14:36:16  thiessen
* restructure macros
*
* Revision 1.2  2003/11/21 15:26:12  thiessen
* add Assign test
*
* Revision 1.1  2003/11/21 14:48:51  thiessen
* initial checkin
*
*/
