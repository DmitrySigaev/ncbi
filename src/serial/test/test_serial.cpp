#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objistrb.hpp>
#include <serial/objostrb.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>
#include "webenv.hpp"

#include <asn.h>
#include <webenv.h>

#include <fstream>

int main(int argc, char** argv)
{
    CTestSerial().AppMain(argc, argv);
	return 0;
}

int CTestSerial::Run(void)
{
    ofstream diag("test.log");
    SetDiagStream(&diag);
    try {
        WebEnv* env = 0;
        {
            {
                CObjectIStreamAsn(ifstream("webenv.ent")).
                    Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                CObjectOStreamAsnBinary(ofstream("webenv.bino",
                                                 ios::out | ios::binary)).
                    Write(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                CObjectIStreamAsnBinary(ifstream("webenv.bin",
                                                 ios::in | ios::binary)).
                    Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                CObjectOStreamAsn(ofstream("webenv.ento")).
                    Write(&env, GetSequenceTypeRef(&env).Get());
            }
        }
        

        CSerialObject write;
        CSerialObject write1;

        write.m_Name = "name";
        write.m_NamePtr = &write1.m_Name;
        write.m_Size = -1;
        write.m_Attributes.push_back("m_Attributes");
        write.m_Attributes.push_back("m_Size");
        write.m_Attributes.push_back("m_");
        write.m_Next = &write1;
        const char* s = "data";
        write.m_Data.assign(s, s + 4);
        write.m_Offsets.push_back(25);
        write.m_Offsets.push_back(-1024);
        write.m_Names[0] = "zero";
        write.m_Names[1] = "one";
        write.m_Names[2] = "two";
        write.m_Names[3] = "three";
        write.m_Names[10] = "ten";

        write1.m_Name = "write1";
        write1.m_NamePtr = new string("test");
        write1.m_Size = 0x7fffffff;
        write1.m_Attributes.push_back("write1");
        write1.m_Next = &write1;

        {
            {
                CObjectOStreamAsn(ofstream("test.asno")) << write;
            }
            {
                CObjectOStreamBinary(ofstream("test.bin",
											  ios::binary)) << write;
            }
            CSerialObject read;
            {
                CObjectIStreamBinary(ifstream("test.bin",
											  ios::binary)) >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
        }

        {
            {
                CObjectOStreamAsnBinary(ofstream("test.asnb",
											     ios::binary)) << write;
            }
            CSerialObject read;
            {
                CObjectIStreamAsn(ifstream("test.asn")) >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
        }
    }
    catch (exception& e) {
        ERR_POST(typeid(e).name() << ": " << e.what());
    }
    catch (...) {
        ERR_POST("Unknown error");
    }
    SetDiagStream(&NcbiCerr);
    return 0;
}
