#include <ncbiconf.h>
#if HAVE_NCBI_C
#include "cppwebenv.hpp"
#include <serial/serialimpl.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>

#include <asn.h>
#include "twebenv.h"

USING_NCBI_SCOPE;

BEGIN_STRUCT_INFO2("Web-Env", Web_Env)
    ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
    ADD_ASN_MEMBER(db_Env, SetOf)->SetOptional();
    ADD_ASN_MEMBER(queries, SequenceOf)->SetOptional();
END_STRUCT_INFO

BEGIN_STRUCT_INFO2("Db-Env", Db_Env)
    ADD_STD_MEMBER(name);
    ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
    ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
//    ADD_ASN_MEMBER(clipboard, Sequence)->SetOptional();
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Argument)
    ADD_STD_MEMBER(name);
    ADD_STD_MEMBER(value);
END_STRUCT_INFO

BEGIN_STRUCT_INFO2("Query-History", Query_History)
    ADD_STD_MEMBER(name)->SetOptional();
    ADD_STD_MEMBER(seqNumber);
    ADD_CHOICE_MEMBER(time, Time);
    ADD_CHOICE_MEMBER(command, Query_Command);
END_STRUCT_INFO

BEGIN_CHOICE_INFO2("Query-Command", Query_Command)
    ADD_CHOICE_VARIANT(search, Sequence, Query_Search);
    ADD_CHOICE_VARIANT(select, Sequence, Query_Select);
    ADD_CHOICE_VARIANT(related, Sequence, Query_Related);
END_CHOICE_INFO

BEGIN_STRUCT_INFO2("Query-Search", Query_Search)
    ADD_STD_MEMBER(db);
    ADD_STD_MEMBER(term);
    ADD_STD_MEMBER(field)->SetOptional();
    ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
    ADD_STD_MEMBER(count);
	ADD_STD_MEMBER(flags)->SetOptional();
END_STRUCT_INFO

BEGIN_STRUCT_INFO2("Query-Select", Query_Select)
    ADD_STD_MEMBER(db);
    ADD_ASN_MEMBER(items, Sequence);
END_STRUCT_INFO

BEGIN_CHOICE_INFO(Query_Related_items)
    ADD_CHOICE_VARIANT(items, Sequence, Item_Set);
    ADD_CHOICE_STD_VARIANT(itemCount, int);
END_CHOICE_INFO

BEGIN_STRUCT_INFO2("Query-Related", Query_Related)
    ADD_CHOICE_MEMBER(base, Query_Command);
    ADD_STD_MEMBER(relation);
    ADD_STD_MEMBER(db);
    ADD_CHOICE_MEMBER2("items", Items_items, Query_Related_items);
END_STRUCT_INFO

BEGIN_STRUCT_INFO2("Filter-Value", Filter_Value)
    ADD_STD_MEMBER(name);
    ADD_STD_MEMBER(value);
END_STRUCT_INFO

BEGIN_CHOICE_INFO(Time)
    ADD_CHOICE_STD_VARIANT(unix, int);
    ADD_CHOICE_VARIANT(full, Sequence, Full_Time);
END_CHOICE_INFO

BEGIN_STRUCT_INFO2("Full-Time", Full_Time)
    ADD_STD_MEMBER(year);
    ADD_STD_MEMBER(month);
    ADD_STD_MEMBER(day);
    ADD_STD_MEMBER(hour);
    ADD_STD_MEMBER(minute);
    ADD_STD_MEMBER(second);
END_STRUCT_INFO

BEGIN_STRUCT_INFO2("Item-Set", Item_Set)
    ADD_ASN_MEMBER(items, OctetString);
    ADD_STD_MEMBER(count);
END_STRUCT_INFO

#endif
