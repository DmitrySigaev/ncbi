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
* Author: Eugene Vasilchenko
*
* File Description:
*   main datatool file: argument processing and task manager
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

#include <memory>

#include <serial/datatool/fileutil.hpp>
#include <serial/datatool/lexer.hpp>
#include <serial/datatool/dtdlexer.hpp>
#include <serial/datatool/parser.hpp>
#include <serial/datatool/dtdparser.hpp>
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/generate.hpp>
#include <serial/datatool/datatool.hpp>
#include <serial/objostrxml.hpp>

BEGIN_NCBI_SCOPE

int CDataTool::Run(void)
{
    if ( !ProcessModules() )
        return 1;
    if ( !ProcessData() )
        return 1;
    if ( !GenerateCode() )
        return 1;

    return 0;
}


void CDataTool::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("datatool", "work with ASN.1/XML data");

    // module arguments
    d->AddKey("m", "moduleFile",
              "module file(s)",
              CArgDescriptions::eInputFile);
    d->AddDefaultKey("M", "externalModuleFile",
                     "external module file(s)",
                     CArgDescriptions::eInputFile, NcbiEmptyString);
    d->AddFlag("i",
               "ignore unresolved symbols");
    d->AddOptionalKey("f", "moduleFile",
                      "write ASN.1 module file",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("fx", "dtdFile",
                      "write DTD file (\"-fx m\" writes modular DTD file)",
                      CArgDescriptions::eOutputFile);

    // data arguments
    d->AddOptionalKey("v", "valueFile",
                      "read value in ASN.1 text format",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("vx", "valueFile",
                      "read value in XML format",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("d", "valueFile",
                      "read value in ASN.1 binary format (-t is required)",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("t", "type",
                      "binary value type (see \"-d\" argument)",
                      CArgDescriptions::eString);
    d->AddOptionalKey("dn", "filename",
                      "DTD module name in XML header (no extension)",
                      CArgDescriptions::eString);
    d->AddFlag("F",
               "read value completely into memory");
    d->AddOptionalKey("p", "valueFile",
                      "write value in ASN.1 text format",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("px", "valueFile",
                      "write value in XML format",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("e", "valueFile",
                      "write value in ASN.1 binary format",
                      CArgDescriptions::eOutputFile);

    // code generation arguments
    d->AddOptionalKey("od", "defFile",
                      "code definition file",
                      CArgDescriptions::eInputFile);
    d->AddFlag("odi",
               "ignore absent code definition file");
    d->AddOptionalKey("of", "listFile",
                      "write list of generated C++ files",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("oc", "basename",
                      "write combining C++ files",
                      CArgDescriptions::eString);
    d->AddFlag("oA",
               "generate C++ files for all types");
    d->AddOptionalKey("ot", "types",
                      "generate C++ files for listed types",
                      CArgDescriptions::eString);
    d->AddOptionalKey("ox", "types",
                      "exclude listed types from generation",
                      CArgDescriptions::eString);
    d->AddFlag("oX",
               "turn off recursive type generation");

    d->AddOptionalKey("on", "namespace",
                      "default namespace", 
                      CArgDescriptions::eString);

    d->AddOptionalKey("opm", "directory",
                      "directory for searching source modules",
                      CArgDescriptions::eString);
    d->AddOptionalKey("oph", "directory",
                      "directory for generated *.hpp files",
                      CArgDescriptions::eString);
    d->AddOptionalKey("opc", "directory",
                      "directory for generated *.cpp files",
                      CArgDescriptions::eString);

    d->AddOptionalKey("or", "prefix",
                      "add prefix to generated file names",
                      CArgDescriptions::eString);
    d->AddFlag("orq",
               "use quoted syntax form for generated include files");
    d->AddFlag("ors",
               "add source file dir to generated file names");
    d->AddFlag("orm",
               "add module name to generated file names");
    d->AddFlag("orA",
               "combine all -or* prefixes");
    d->AddOptionalKey("oR", "rootDirectory",
                      "set \"-o*\" arguments for NCBI directory tree",
                      CArgDescriptions::eString);

    d->AddFlag("lax_syntax",
               "allow non-standard ASN.1 syntax accepted by asntool");

    SetupArgDescriptions(d.release());
}

bool CDataTool::ProcessModules(void)
{
    const CArgs& args = GetArgs();

    list<string> modulesPath;

    if ( const CArgValue& oR = args["oR"] ) {
        // NCBI directory tree
        const string& rootDir = oR.AsString();
        generator.SetHPPDir(Path(rootDir, "include"));
        string srcDir = Path(rootDir, "src");
        generator.SetCPPDir(srcDir);
        modulesPath.push_back(srcDir);
        generator.SetFileNamePrefixSource(eFileName_FromSourceFileName);
        generator.SetDefaultNamespace("NCBI_NS_NCBI::objects");
    }
    
    if ( const CArgValue& opm = args["opm"] ) {
        modulesPath.clear();
        NStr::Split(opm.AsString(), ",", modulesPath);
    }
    
    LoadDefinitions(generator.GetMainModules(),
                    modulesPath, args["m"].AsString());

    if ( const CArgValue& f = args["f"] ) {
        generator.GetMainModules().PrintASN(f.AsOutputFile());
        f.CloseFile();
    }

    if ( const CArgValue& fx = args["fx"] ) {
        if ( fx.AsString() == "m" )
            generator.GetMainModules().PrintDTDModular();
        else {
            generator.GetMainModules().PrintDTD(fx.AsOutputFile());
            fx.CloseFile();
        }
    }
    
    LoadDefinitions(generator.GetImportModules(),
                    modulesPath, args["M"].AsString());

    if ( !generator.Check() ) {
        if ( !args["i"] ) { // ignored
            ERR_POST("some types are unknown");
            return false;
        }
        else {
            ERR_POST(Warning << "some types are unknown: ignoring");
        }
    }
    return true;
}

bool CDataTool::ProcessData(void)
{    
    const CArgs& args = GetArgs();

    // convert data
    ESerialDataFormat inFormat;
    string inFileName;
    const CArgValue& t = args["t"];
    
    if ( const CArgValue& v = args["v"] ) {
        inFormat = eSerial_AsnText;
        inFileName = v.AsString();
    }
    else if ( const CArgValue& vx = args["vx"] ) {
        inFormat = eSerial_Xml;
        inFileName = vx.AsString();
    }
    else if ( const CArgValue& d = args["d"] ) {
        if ( !t ) {
            ERR_POST("ASN.1 value type must be specified (-t)");
            return false;
        }
        inFormat = eSerial_AsnBinary;
        inFileName = d.AsString();
    }
    else // no input data
        return true;

    auto_ptr<CObjectIStream>
        in(CObjectIStream::Open(inFormat, inFileName, eSerial_StdWhenAny));

    string typeName;
    if ( t ) {
        typeName = t.AsString();
        in->ReadFileHeader();
    }
    else {
        typeName = in->ReadFileHeader();
    }

    TTypeInfo typeInfo =
        generator.GetMainModules().ResolveInAnyModule(typeName, true)->
        GetTypeInfo().Get();
    
    // determine output data file
    ESerialDataFormat outFormat;
    string outFileName;
    
    if ( const CArgValue& p = args["p"] ) {
        outFormat = eSerial_AsnText;
        outFileName = p.AsString();
    }
    else if ( const CArgValue& px = args["px"] ) {
        outFormat = eSerial_Xml;
        outFileName = px.AsString();
    }
    else if ( const CArgValue& e = args["e"] ) {
        outFormat = eSerial_AsnBinary;
        outFileName = e.AsString();
    }
    else {
        // no input data
        outFormat = eSerial_None;
    }

    if ( args["F"] ) {
        // read fully in memory
        AnyType value;
        in->Read(&value, typeInfo, CObjectIStream::eNoFileHeader);
        if ( outFormat != eSerial_None ) {
            // store data
            auto_ptr<CObjectOStream>
                out(CObjectOStream::Open(outFormat, outFileName,
                                         eSerial_StdWhenAny));
            if ( outFormat == eSerial_Xml ) {
                // Set DTD file name (default prefix is added in any case)
                if( const CArgValue& dn = args["dn"] ) {
                  dynamic_cast<CObjectOStreamXml*>(out.get())->
                      SetDTDFileName(dn.AsString());
                }
            }
            out->Write(&value, typeInfo);
        }
    }
    else {
        if ( outFormat != eSerial_None ) {
            // copy
            auto_ptr<CObjectOStream>
                out(CObjectOStream::Open(outFormat, outFileName,
                                         eSerial_StdWhenAny));
            if ( outFormat == eSerial_Xml ) {
                // Set DTD file name (default prefix is added in any case)
                if( const CArgValue& dn = args["dn"] ) {
                  dynamic_cast<CObjectOStreamXml*>(out.get())->
                      SetDTDFileName(dn.AsString());
                }
            }
            CObjectStreamCopier copier(*in, *out);
            copier.Copy(typeInfo, CObjectStreamCopier::eNoFileHeader);
        }
        else {
            // skip
            in->Skip(typeInfo, CObjectIStream::eNoFileHeader);
        }
    }
    return true;
}

bool CDataTool::GenerateCode(void)
{
    const CArgs& args = GetArgs();

    // load generator config
    if ( const CArgValue& od = args["od"] )
        generator.LoadConfig(od.AsString(), args["odi"]);
    //if ( const CArgValue& oD = args["oD"] )
    //    generator.AddConfigLine(oD.AsString());

    // set list of types for generation
    if ( args["oX"] )
        generator.ExcludeRecursion();
    if ( args["oA"] )
        generator.IncludeAllMainTypes();
    if ( const CArgValue& ot = args["ot"] )
        generator.IncludeTypes(ot.AsString());
    if ( const CArgValue& ox = args["ox"] )
        generator.ExcludeTypes(ox.AsString());

    if ( !generator.HaveGenerateTypes() )
        return true;

    // prepare generator
    
    // set namespace
    if ( const CArgValue& on = args["on"] )
        generator.SetDefaultNamespace(on.AsString());
    
    // set output files
    if ( const CArgValue& oc = args["oc"] ) {
        const string& fileName = oc.AsString();
        generator.SetCombiningFileName(fileName);
        generator.SetFileListFileName(fileName+".files");
    }
    if ( const CArgValue& of = args["of"] )
        generator.SetFileListFileName(of.AsString());
    
    // set directories
    if ( const CArgValue& oph = args["oph"] )
        generator.SetHPPDir(oph.AsString());
    if ( const CArgValue& opc = args["opc"] )
        generator.SetCPPDir(opc.AsString());
    
    // set file names prefixes
    if ( const CArgValue& orF = args["or"] )
        generator.SetFileNamePrefix(orF.AsString());
    if ( args["orq"] )
        generator.UseQuotedForm(true);
    if ( args["ors"] )
        generator.SetFileNamePrefixSource(eFileName_FromSourceFileName);
    if ( args["orm"] )
        generator.SetFileNamePrefixSource(eFileName_FromModuleName);
    if ( args["orA"] )
        generator.SetFileNamePrefixSource(eFileName_UseAllPrefixes);
    
    // generate code
    generator.GenerateCode();
    return true;
}


void CDataTool::LoadDefinitions(CFileSet& fileSet,
                                const list<string>& modulesPath,
                                const string& nameList)
{
    list<string> names;
    NStr::Split(nameList, " ", names);
    iterate ( list<string>, fi, names ) {
        const string& name = *fi;
        if ( !name.empty() ) {
            SourceFile fName(name, modulesPath);
            switch (fName.GetType()) {
            default:
                // throw exception here
                break;
            case SourceFile::eASN:
                {
                    ASNLexer lexer(fName);
                    lexer.AllowIDsEndingWithMinus(GetArgs()["lax_syntax"]);
                    ASNParser parser(lexer);
                    fileSet.AddFile(parser.Modules(name));
                }
                break;
            case SourceFile::eDTD:
                {
                    DTDLexer lexer(fName);
                    DTDParser parser(lexer);
                    fileSet.AddFile(parser.Modules(name));
                }
                break;
            }
        }
    }
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;
    return CDataTool().AppMain(argc, argv, 0, eDS_Default, 0, "datatool");
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.56  2002/10/22 15:06:12  gouriano
* added possibillity to use quoted syntax form for generated include files
*
* Revision 1.55  2002/10/15 13:57:09  gouriano
* recognize DTD files and handle them using DTD lexer/parser
*
* Revision 1.54  2002/09/26 16:57:31  vasilche
* Added flag for compatibility with asntool
*
* Revision 1.53  2002/08/06 17:03:48  ucko
* Let -opm take a comma-delimited list; move relevant CVS logs to end.
*
* Revision 1.52  2001/12/13 19:32:41  juran
* Make main()'s argv of type (const char*), for use with jTools.
*
* Revision 1.51  2001/10/17 18:19:52  grichenk
* Updated to use CObjectOStreamXml::GetFileXXX
*
* Revision 1.50  2001/04/16 17:53:45  kholodov
* Added: Option -dn to specify the DTD file name in XML header
*
* Revision 1.49  2000/12/26 22:24:53  vasilche
* Updated for new behaviour of CNcbiArgs.
*
* Revision 1.48  2000/12/12 17:57:06  vasilche
* Avoid using of new C++ keywords ('or' in this case).
*
* Revision 1.47  2000/12/12 14:28:17  vasilche
* Changed the way arguments are processed.
*
* Revision 1.46  2000/11/27 18:19:47  vasilche
* Datatool now conforms CNcbiApplication requirements.
*
* Revision 1.45  2000/11/20 17:26:32  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.44  2000/11/08 17:50:18  vasilche
* Fixed compilation error on MSVC.
*
* Revision 1.43  2000/11/08 17:02:51  vasilche
* Added generation of modular DTD files.
*
* Revision 1.42  2000/11/01 20:38:59  vasilche
* OPTIONAL and DEFAULT are not permitted in CHOICE.
* Fixed code generation for DEFAULT.
*
* Revision 1.41  2000/09/29 16:18:28  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.40  2000/09/26 17:38:26  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.39  2000/09/18 20:00:28  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.38  2000/09/18 13:53:00  vasilche
* Fixed '.files' extension.
*
* Revision 1.37  2000/09/01 13:16:27  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.36  2000/08/25 15:59:20  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.35  2000/07/10 17:32:00  vasilche
* Macro arguments made more clear.
* All old ASN stuff moved to serialasn.hpp.
* Changed prefix of enum info functions to GetTypeInfo_enum_.
*
* Revision 1.34  2000/06/16 16:31:39  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.33  2000/05/24 20:09:29  vasilche
* Implemented DTD generation.
*
* Revision 1.32  2000/04/28 16:58:16  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.31  2000/04/13 17:30:37  vasilche
* Avoid INTERNAL COMPILER ERROR on MSVC.
* Problem is in "static inline" function which cannot be expanded inline.
*
* Revision 1.30  2000/04/13 14:50:36  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.29  2000/04/12 15:36:51  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.28  2000/04/07 19:26:28  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.27  2000/03/10 17:59:31  vasilche
* Fixed error reporting.
*
* Revision 1.26  2000/02/01 21:48:01  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.25  2000/01/06 16:13:43  vasilche
* Updated help messages.
*
* Revision 1.24  1999/12/30 21:33:39  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.23  1999/12/28 18:55:58  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.22  1999/12/21 17:18:35  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.21  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* Revision 1.20  1999/12/01 17:36:26  vasilche
* Fixed CHOICE processing.
*
* Revision 1.19  1999/11/15 20:01:34  vasilche
* Fixed GCC error
*
* Revision 1.18  1999/11/15 19:36:16  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
