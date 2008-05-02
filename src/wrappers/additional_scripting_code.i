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
 * Authors:  Josh Cherry
 *
 * File Description:  Perl and Python code for insertion
 *
 */


#ifdef SWIGPYTHON

#ifdef FOR_OUTSIDERS

%pythoncode %{

_within_ncbi = False
_use_preprocessed_specs = True

%}

#else

%pythoncode %{

_within_ncbi = True
_use_preprocessed_specs = False

%}

#endif

%pythoncode %{

# dynamic casting
def dynamic_cast(new_class, obj):
    return new_class.__dynamic_cast_to__(obj)


# Downcast an object as far down as possible;
# may be very slow
def Downcast(obj):
    for subclass in obj.__class__.__subclasses__():
        if not subclass.__subclasses__():
            # a *Ptr class
            continue
        dc = dynamic_cast(subclass, obj)
        if dc:
            return Downcast(dc)
    # no successful downcasts
    return obj


# Downcast a CSerialObject as far as possible,
# based on type info.  This should be faster
# than Downcast().
def DowncastSerialObject(so):
    obj_type_name = so.GetThisTypeInfo().GetName()
    cname = 'C' + obj_type_name.replace('-', '_')
    # class need not be in this module; look through all modules
    import sys
    klass = None
    for key in sys.modules.keys():
        module = sys.modules[key]
        if module == None:
            # some are like this under Windows
            continue
        d = module.__dict__
        if d.has_key(cname):
            if issubclass(d[cname], CSerialObject):
                klass = d[cname]
                break
    if klass == None:
        raise RuntimeError, 'could not downcast to %s: class not found' \
              % cname
    return dynamic_cast(klass, so)


# read any asn or xml file

def ReadAsnFile(fname, obj_type = None):
    '''
    Read a text ASN.1 file containing an object of any type known to this
    code module.  The ASN.1 type can optionally be specified in the
    obj_type parameter.  If it is omitted, the type will be determined
    from the file header.  If supplied, it must match the actual type of
    the object in the file or an exception will be raised.

    examples:

    obj = ReadAsnFile('myfile.prt')                       # reads any type
    ch22 = ReadAsnFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.prt', 'Seq-entry') # expects a Seq-entry
    '''
    return ReadSerialFile(fname, eSerial_AsnText, obj_type)

def ReadXmlFile(fname, obj_type = None):
    '''
    Read an xml file containing an ncbi serial object of any type known
    to this code module.  The serial type can optionally be specified in
    the obj_type parameter.  If it is omitted, the type will be determined
    from the file header.  If supplied, it must match the actual type of
    the object in the file or an exception will be raised.

    examples:

    obj = ReadXmlFile('myfile.xml')                       # reads any type
    ch22 = ReadXmlFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.xml', 'Seq-entry') # expects a Seq-entry
    '''
    return ReadSerialFile(fname, eSerial_Xml, obj_type)

def ReadAsnBinFile(fname, obj_type):
    '''
    Read a binary ASN.1 file containing an object of any type known to
    this code module.  The ASN.1 type MUST be specified in the obj_type
    parameter.

    example:

    # read a Seq-entry stored in ASN.1 binary
    ch22 = ReadAsnBinFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.val', 'Seq-entry')
    '''
    return ReadSerialFile(fname, eSerial_AsnBinary, obj_type)

def ReadSerialFile(fname, serial_format, obj_type = None):
    '''
    Read a file containing an ncbi serial object in text or binary ASN.1
    or xml.  The serial_format parameter is eSerial_AsnText,
    eSerial_AsnBinary, or eSerial_Xml.  obj_type is a string giving
    the object type (e.g., 'Seq-loc').  It may be omitted for text ASN.1
    and xml, but must be supplied for ASN.1 binary.

    examples:
    
    ch22 = ReadSerialFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.prt', eSerial_AsnText, 'Seq-entry')
    ch22 = ReadSerialFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.val', eSerial_AsnBinary, 'Seq-entry')
    ch22 = ReadSerialFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.xml', eSerial_Xml, 'Seq-entry')
    obj = ReadSerialFile('myfile.prt', eSerial_AsnText)
    obj = ReadSerialFile('myfile.xml', eSerial_Xml)
    '''
    objistr = CObjectIStream.Open(serial_format, fname)
    if obj_type == None:
        if serial_format == eSerial_AsnBinary:
            raise RuntimeError, 'ASN.1 type must be specified for read of ' \
                               'binary file %s' % fname
        obj_type_name = objistr.ReadFileHeader()
    else:
        obj_type_name = obj_type
    cname = 'C' + obj_type_name.replace('-', '_')
    # class need not be in this module; look through all modules
    import sys
    obj = None
    for key in sys.modules.keys():
        module = sys.modules[key]
        if module == None:
            # some are like this under Windows
            continue
        d = module.__dict__
        if d.has_key(cname):
            if issubclass(d[cname], CSerialObject):
                obj = d[cname]()	
                break
    if obj == None:
        raise RuntimeError, 'class %s not found while reading a %s' \
              % (cname, obj_type_name)

    if obj_type == None:
        objistr.Read(obj, obj.GetThisTypeInfo(), objistr.eNoFileHeader)
    else:
        objistr.Read(obj, obj.GetThisTypeInfo())
    objistr.Close()
    return obj


# tree view of serial objects using kxmleditor
def SerialObjectTreeView(sobj, *args):
    import os, tempfile
    caption_switch = ''
    if len(args) > 0:
        if len(args) > 1:
            raise RuntimeError("takes 1 or 2 arguments: %d given"
                               % len(args + 1))
        caption_switch = '--caption \'' + args[0] + '\' '
    fname = tempfile.mktemp()
    fid = open(fname, 'w')
    fid.write(sobj.Xml())
    fid.close()
    os.system('kxmleditor %s%s >& /dev/null || '
              '/usr/local/kxmleditor/bin/kxmleditor %s >& /dev/null &'
               % (caption_switch, fname, fname) )

CSerialObject.TreeView = SerialObjectTreeView


def SerialObjectTextView(sobj):
    CTextEditor.DisplayText(sobj.Asn())

CSerialObject.TextView = SerialObjectTextView

def SerialObjectTextEdit(sobj):
    s = string()
    CTextEditor.EditText(sobj.Asn(), s)
    rv = sobj.__class__()
    rv.FromAsn(s)
    return rv

CSerialObject.TextEdit = SerialObjectTextEdit


def SerialClone(sobj):
    rv = type(sobj)()
    rv.Assign(sobj)
    return rv

CSerialObject.Clone = SerialClone


if _within_ncbi:
    doxy_url_base = 'http://intranet.ncbi.nlm.nih.gov/' \
                    'ieb/ToolBox/CPP_DOC/doxyhtml/'
else:
    doxy_url_base = 'http://www.ncbi.nlm.nih.gov/' \
                    'ieb/ToolBox/CPP_DOC/doxyhtml/'

def Doxy(arg):
    '''
    Show Doxygen-generated documentation of class in a web browser
    '''
    if isinstance(arg, type):
        # a Python class
        class_name = arg.__name__
    elif isinstance(arg, str):
        # a string?
        class_name = arg
    elif isinstance(arg, object):
        # an instance of a "new-style" class
        class_name = arg.__class__.__name__
    else:
        msg = "Doxy: couldn't figure out a class name from the argument"
        raise RuntimeError(msg)

    mangled_class_name = class_name.replace('_', '__')
    if class_name[0] == 'S':
        class_or_struct = 'struct'
    else:
        class_or_struct = 'class'
    import webbrowser
    url = doxy_url_base + class_or_struct + mangled_class_name + '.html'
    webbrowser.open_new(url)

ncbi_object.Doxy = classmethod(Doxy)


# Similar for LXR
if _within_ncbi:
    lxr_url_base = 'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/'
else:
    lxr_url_base = 'http://www.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/'

def Lxr(arg):
    '''
    Show LXR-generated documentation of class in a web browser
    '''
    if isinstance(arg, type):
        # a Python class
        class_name = arg.__name__
    elif isinstance(arg, str):
        # a string?
        class_name = arg
    elif isinstance(arg, object):
        # an instance of a "new-style" class
        class_name = arg.__class__.__name__
    else:
        msg = "Lxr: couldn't figure out a symbol name from the argument"
        raise RuntimeError(msg)

    import webbrowser
    url = lxr_url_base + 'ident?i=' + class_name
    webbrowser.open_new(url)

ncbi_object.Lxr = classmethod(Lxr)

# Doxygen search; take a string, bring up url that searches for it.
if _within_ncbi:
    def DoxySearch(name):
        '''
        Search Doxygen-generated documentation for a name,
        showing result in a web browser
        '''
        import webbrowser
        url = doxy_url_base + 'search.php?query=' + name
        webbrowser.open_new(url)


try:
    import find_asn_spec
except:
    pass

def FindSpec(type_name):
    if not _use_preprocessed_specs:
        return find_asn_spec.FindSpec(type_name)
    else:
        import os
        fname = os.path.join(os.path.split(__file__)[0], 'ncbi_asn_specs')

        fid = open(fname)
        for line in fid:
            if line.strip() == '':
                # blank line ends table
                return None
            ty, start, length = line.split()
            if ty == type_name:
                fid.seek(-int(start), 2)
                spec = fid.read(int(length))
                return spec
    

if _within_ncbi:
    asn_spec_url_base = 'http://intranet.ncbi.nlm.nih.gov' \
                        '/ieb/ToolBox/CPP_DOC/asn_spec/'
else:
    asn_spec_url_base = 'http://www.ncbi.nlm.nih.gov' \
                        '/ieb/ToolBox/CPP_DOC/asn_spec/'

def Spec(arg, web=False):
   if isinstance(arg, CSerialObject) \
   or (isinstance(arg, type) and issubclass(arg, CSerialObject)):
      # a serial object instance or class;
      # get type string from type info
      type_name = arg.GetTypeInfo().GetName()
   else:
      # a string, one hopes
      type_name = arg
   if not web:
      spec = FindSpec(type_name)
      if spec == None:
         print 'ASN.1 spec. not found for type %s' % type_name
      else:
         print spec
   else:
      url = asn_spec_url_base + type_name + '.html'
      import webbrowser
      webbrowser.open_new(url)
      
CSerialObject.Spec = classmethod(Spec)


def EntrezUrl(ids, db=None):

    def GetUid(id, id1cli):
        if isinstance(id, int):
            return id
        if isinstance(id, string):
            id = str(id)
        if isinstance(id, str):
            try:
                gi = int(id)
                return gi
            except:
                seq_id = CSeq_id(id)
        elif isinstance(id, CSeq_id):
            seq_id = id
        else:
            raise RuntimeError("couldn't convert argument to a gi")

        return id1cli.AskGetgi(seq_id)

    id1cli = CID1Client()

    if isinstance(ids, str) or isinstance(ids, int) \
           or isinstance(ids, string) \
           or isinstance(ids, CSeq_id):
        ids = [ids]

    ids = ids[:]   # in case we have a std::list, which can't be indexed
    if db == None:
        # Assume it should be 'protein' or 'nucleotide';
        # figure out which from the *first* gi
        hand = CSimpleOM.GetBioseqHandle(GetUid(ids[0], id1cli))
        if hand.GetBioseqMolType() == CSeq_inst.eMol_aa:
            db = 'protein'
        else:
            db = 'nucleotide'

    id_list = ','.join([str(GetUid(id, id1cli)) for id in ids])  # comma-separated
    
    if db == 'pubmed':
        url = 'http://www.ncbi.nlm.nih.gov/pubmed/%s?dopt=docsum' % id_list
    else:
        base_url = 'http://www.ncbi.nlm.nih.gov/sites'
        query_start = '/entrez?db=%s&cmd=Retrieve&list_uids=' % db
        url = base_url + query_start + id_list

    return url


def EntrezWeb(ids, db=None):
    import webbrowser
    webbrowser.open_new(EntrezUrl(ids, db))


# Module introspection

import types

def count_methods(classes):
    'Given a sequence of class names, count the methods'
    count = 0
    the_dict = globals()
    for name in classes:
        cl = the_dict[name]
        count += len([attr for attr in cl.__dict__.values()
                      if type(attr) == types.FunctionType
                      or type(attr) == staticmethod])
    return count


def info(detailed=False):
    'Print a bunch of information about classes and their methods'

    serial_objects = []
    cobjects = []
    classes = []
    asn_modules = {}

    the_dict = globals()
    for cname in the_dict:
        if isinstance(the_dict[cname], type) \
           and not (cname.endswith('Ptr') and cname[:-3] in the_dict):
            the_class = the_dict[cname]
            if issubclass(the_class, CSerialObject):
                serial_objects.append(cname)
                if the_class != CSerialObject:
                    asn_module = the_class.GetTypeInfo().GetModuleName()
                    if not asn_module in asn_modules:
                        asn_modules[asn_module] = []
                    asn_modules[asn_module].append(cname)
            if issubclass(the_class, CObject):
                cobjects.append(cname)
            if issubclass(the_class, ncbi_object):
                classes.append(cname)

    print "%4d classes          (%d methods)" \
          % (len(classes), count_methods(classes))
    print "%4d CObject's        (%d methods)" \
          % (len(cobjects), count_methods(cobjects))
    print "%4d CSerialObject's  (%d methods)" \
          % (len(serial_objects), count_methods(serial_objects))

    if detailed:
        print 'by module:'
        for asn_module in sorted(asn_modules):
            print '%3d %-21s (%d methods)' \
                  % (len(asn_modules[asn_module]), asn_module,
                     count_methods(asn_modules[asn_module]))

    type_counts = {}
    for name in dir(_ncbi):
        item = getattr(_ncbi, name)
        the_type = type(item)
        if not the_type in type_counts:
            type_counts[the_type] = 0
        type_counts[the_type] += 1

    print '\n_ncbi members:'
    total = 0
    for key in type_counts:
        print '%5d %s' % (type_counts[key], key)
        total += type_counts[key]
    print '%5d %s' % (total, 'Total')


def classes():
    'Return a list of class names'
    rv = {}
    the_dict = globals()
    for cname in the_dict:
        if isinstance(the_dict[cname], type) \
           and not (cname.endswith('Ptr') and cname[:-3] in the_dict):
            the_class = the_dict[cname]
            rv[cname] = the_class
    return rv


# Search ncbi namespace for names containing a pattern
import re
def Search(pattern, deep=False):
    names = globals().keys()
    names.sort()
    for name in names:
        if re.search(pattern.lower(), name.lower()):
            if name.endswith('__dynamic_cast_to__'):
                continue
            if name.endswith('Ptr') and name[:-3] in names:
                # filter out SWIG Ptr classes
                continue
            print name
        if deep and type(globals()[name]) == type(object):
            for mem_name in globals()[name].__dict__.keys():
                if re.search(pattern.lower(), mem_name.lower()):
                    print '%s.%s' % (name, mem_name)


def _class_monitoring_init(self, *args):
    global class_monitor_info
    cname = self.__class__.__name__
    if cname.endswith('Ptr') and len(self.__class__.__subclasses__()) == 0:
        cname = cname[0:-3]
    if cname in class_monitor_info:
        class_monitor_info[cname] += 1
    else:
        class_monitor_info[cname] = 1
    self.__orig_init__(*args)


def recursive_modify(base_class):
    if not base_class.__dict__.has_key('__orig_init__'):
        # modify only if we haven't already been here
        # (multiple inheritance issue)
        base_class.__orig_init__ = base_class.__init__
        base_class.__init__ = _class_monitoring_init
    for sub_class in base_class.__subclasses__():
        recursive_modify(sub_class)
 

def enable_class_monitoring():
    global class_monitor_info
    class_monitor_info = {}
    recursive_modify(ncbi_object)    


# no-op implementation to ease switching between ncbi and ncbi_fast
def use_class(arg):
    pass


# after a CException, examine the source code around where it was thrown
def li(arg=False):
    do_vt100_bolding = True
    # see http://www.ecma-international.org/publications/standards/Ecma-048.htm
    # and http://www.cs.uu.nl/wais/html/na-dir/emulators-faq/part3.html
    import sys
    import re
    s = sys.last_value.args[0]
    m = re.match(r'NCBI C\+\+ Exception:\n *\"(.*)\", line ([0-9]*)', s)
    fname, lineno = m.groups()
    lineno = int(lineno)
    #print fname, lineno
    if arg == 'cvs':
        # show cvs web page
        if fname.find('/src/') != -1:
            short_fname = fname[fname.find('/src/') + 1:]
        elif fname.find('/include/') != -1:
            short_fname = fname[fname.find('/include/') + 1:]
        else:
            raise RuntimeError('cannot figure out correct relative path for %s'
                               % fname)
        CvsWeb(short_fname)
    elif arg == True:
        # display source file in text editor
        CTextEditor.EditFile(fname, CTextEditor.eGuess, lineno)
    else:
        # print to stdout
        fid = open(fname)
        lines = fid.readlines()
        fid.close
        for i in range(lineno - 11, lineno + 10):
            if i + 1 == lineno and do_vt100_bolding:
                sys.stdout.write('\x1b[01;31m')
            print i + 1, lines[i],
            if i + 1 == lineno and do_vt100_bolding:
                sys.stdout.write('\x1b[00m')


#del object
%}

#endif

#ifdef SWIGPERL

#ifdef FOR_OUTSIDERS

%pragma(perl5) code = %{

$_within_ncbi = 0;
$_use_preprocessed_specs = 1;

%}

#else

%pragma(perl5) code = %{

$_within_ncbi = 1;
$_use_preprocessed_specs = 0;

############ DoxySearch() -- Search doxygen documentation ##########

# This won't work outside of NCBI, even at www

sub DoxySearch {
    my $name = shift;

    my $doxy_url_base =
        'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/doxyhtml/';
    my $url = $doxy_url_base . 'search.php?query=' . $name;

    _open_url($url);
}

%}

#endif

%pragma(perl5) code = %{


############ dynamic_cast() -- Dynamic typecasting ############

package ncbi;
sub dynamic_cast {
    my $type = shift;
    my $obj = shift;
    my $cmd = $type . '::__dynamic_cast_to__($obj)';
    return eval($cmd);
}


############ _open_url() -- launch url in a web browser ##########

sub _open_url {
    my $url = shift;
    system("python -c 'import sys, webbrowser; webbrowser.open_new(sys.argv[1])' '$url'");
}


############ Spec() -- find and print ASN.1 specification ##########

# Call Python or use pre-processed specs
package ncbi;
sub Spec {
    my $arg = shift;
    my $web = shift;

    my $type;
    if ($arg->isa('HASH')) {
        $type = $arg->GetThisTypeInfo()->GetName();
    } else {
        # A string, we hope
        if (substr($arg, 0, 6) eq 'ncbi::') {
	    # Treat as a qualified class name
            my $obj = new $arg;
            $obj->Spec($web);
            return;
        }
        $type = $arg;
    }

    if (!$web) {
        if (!$_use_preprocessed_specs) {
            my $cmd = "python -c \'\
import sys \
sys.path.append(\"/netopt/ncbi_tools/c++-wrappers/python\") \
import find_asn_spec \
print find_asn_spec.FindSpec(\"$type\") \
\'";
            system($cmd);
        } else {
            my $mod_path = $INC{'ncbi.pm'};
            my $spec_file = substr($mod_path, 0, -7) . 'ncbi_asn_specs';
            my $status = open(FID, $spec_file);
            if (!$status) {
                print "Couldn't open file '$spec_file'\n";
                return;
            }
            while (<FID>) {
                chomp($_);
                if ($_ eq '') {
                    print "Spec for $type not found\n";
                    return;
                }
                my @fields = split(' ', $_);
                my $ty = $fields[0];
                my $start = $fields[1];
                my $length = $fields[2];
                if ($ty eq $type) {
                    seek(FID, -$start, 2);
                    my $spec;
                    read(FID, $spec, $length);
                    print "$spec\n";
                    return;
                }
            }
        }
    } else {
        my $asn_spec_url_base;
        if (!$_within_ncbi) {
            $asn_spec_url_base
                = 'http://www.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/asn_spec/';
        } else {
            $asn_spec_url_base
                = 'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/asn_spec/';
        }
        my $url = $asn_spec_url_base . $type . '.html';
        _open_url($url);
    }
}


############ Doxy() -- Launch doxygen documentation in web browser ##########

sub Doxy {
    my $arg = shift;
    my $type;
    if ($arg->isa('HASH')) {
        $type = ref($arg);
    } else {
        # a string, we hope
        $type = $arg;
    }

    my $doxy_url_base;
    if (!$_within_ncbi) {
        $doxy_url_base
            = "http://www.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/doxyhtml/";
    } else {
        $doxy_url_base
            = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/doxyhtml/";
    }

    my @tmp = split(/::/, $type);
    my $class_name = $tmp[-1];
    $class_name =~ s/_/__/g;
    my $class_or_struct;
    if ($class_name =~ /^S/) {
        $class_or_struct = "struct";
    } else {
        $class_or_struct = "class";
    }
    my $url = $doxy_url_base . $class_or_struct . $class_name . ".html";
    _open_url($url);
}


############ Lxr() -- Launch LXR documentation in web browser ##########

sub Lxr {
    my $arg = shift;
    my $type;
    if ($arg->isa('HASH')) {
        $type = ref($arg);
    } else {
        # a string, we hope
        $type = $arg;
    }

    # chop off any namespace qualifiers
    my @tmp = split(/::/, $type);
    $type = $tmp[-1];

    my $lxr_url_base;
    if (!$_within_ncbi) {
        $lxr_url_base
            = 'http://www.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/';
    } else {
        $lxr_url_base
            = 'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/';
    }
    my $url = $lxr_url_base . 'ident?i=' . $type;
    _open_url($url);
}


############ TreeView() -- Serial object tree view using kxmleditor ##########

package ncbi::CSerialObject;
sub TreeView {
    my $arg = shift;
    my $xml = $arg->Xml();

    # write xml to a temp file
    use Fcntl;
    use POSIX qw(tmpnam);
    do {$fname = tmpnam()}
        until sysopen(FH, $fname, O_RDWR|O_CREAT|O_EXCL);
    print FH $xml;
    close(FH);

    # launch kxmleditor with the temp file as argument
    my $cmd = "kxmleditor $fname >& /dev/null || /usr/local/kxmleditor/bin/kxmleditor $fname >& /dev/null &";
    system($cmd);
}

############ TextView() -- Serial object text view using an editor ##########

package ncbi::CSerialObject;
sub TextView {
    my $sobj = shift;
    ncbi::CTextEditor::DisplayText($sobj->Asn());
}

############ TextEdit() -- Edit serial object as ASN.1 text ##########

package ncbi::CSerialObject;
sub TextEdit {
    my $sobj = shift;
    my $s = new ncbi::string();
    ncbi::CTextEditor::EditText($sobj->Asn(), $s);
    my $type = ref($sobj);
    my $cmd = 'new ' . $type;
    my $rv = eval($cmd);
    $rv->FromAsn($s);
    return $rv;
}

############ SerialClone() -- Make a copy of a serial object ##########

package ncbi;
sub SerialClone {
    my $sobj = shift;
    my $type = ref($sobj);
    my $cmd = 'new ' . $type;
    my $rv = eval($cmd);
    $rv->Assign($sobj);
    return $rv;
}

############ EntrezUrl() -- url for Seq-ids or uids ##########

package ncbi;
sub EntrezUrl {

    sub GetUid {
        (my $id, my $id1cli) = @_;
        my $seq_id;
        if (ref($id) eq 'ncbi::CSeq_id') {
            $seq_id = $id;
        } else {
            # A string?
            use POSIX;
            @res = POSIX::strtol($id);
            if ($res[0] && ($res[1] == 0)) {
                # An integer or a string rep. thereof
                return $res[0];
            }
            $seq_id = new ncbi::CSeq_id($id);
        }
        return $id1cli->AskGetgi($seq_id);
    }

    my $ids = shift;
    my $db = shift;

    my $id1cli = new ncbi::CID1Client();

    my $type = ref($ids);
    if (!$type || $type eq 'ncbi::CSeq_id') {
        $ids = [$ids];
    } elsif ($type ne 'ARRAY') {
        $ids = $ids->array();
    }

    if (!defined($db)) {
        # Assume it should be 'protein' or 'nucleotide';
        # figure out which from the *first* gi
        my $hand = ncbi::CSimpleOM::GetBioseqHandle(GetUid($ids->[0], $id1cli));
        if ($hand->GetBioseqMolType() == $ncbi::CSeq_inst_Base::eMol_aa) {
            $db = 'protein';
        } else {
            $db = 'nucleotide';
        }
    }

    # Make comma-separated uid list
    my $id_list = '';
    my $i = 0;
    foreach $id (@$ids) {
        if ($i > 0) {
            $id_list .= ',';
        }
        $id_list .= GetUid($ids->[$i], $id1cli);
        ++$i;
    }

    my $url;
    if ($db eq 'pubmed') {
        $url = "http://www.ncbi.nlm.nih.gov/pubmed/${id_list}?dopt=docsum"
    } else {
        my $base_url = 'http://www.ncbi.nlm.nih.gov/sites';
        my $query_start = "/entrez?db=${db}&cmd=Retrieve&list_uids=";
        $url = $base_url . $query_start . $id_list;
    }

    return $url;
}

############ EntrezWeb() -- pop up web page for Seq-ids or uids ##########

package ncbi;
sub EntrezWeb {
    my $ids = shift;
    my $db = shift;

    my $url = ncbi::EntrezUrl($ids, $db);

    _open_url($url);
}

############ bool() -- return 1 or 0 depending on boolean value ##########

sub bool {
    if (@_ != 1) {die "bool() expected 1 argument, got " . int(@_);}
    my $arg = shift;
    return $arg ? 1 : 0;
}

%}

%extend ncbi::CSerialObject {
%perlcode %{
*Clone = *ncbi::SerialClone;
%}
};

#endif


#ifdef SWIGRUBY

%inline %{

void Spec(const string& type, bool web = false)
{
    if (web) {
        CExec::SpawnLP(CExec::eWait,
                       "/netopt/ncbi_tools/c++-wrappers/bin/asn_spec",
                       "--web", type.c_str(), NULL);
    } else {
        CExec::SpawnLP(CExec::eWait,
                       "/netopt/ncbi_tools/c++-wrappers/bin/asn_spec",
                       type.c_str(), NULL);
    }
}


void Spec(const CSerialObject& obj, bool web = false)
{
    Spec(obj.GetThisTypeInfo()->GetName(), web);
}

%}


%extend ncbi::CSerialObject {
    void Spec(bool web = false) {
        void Spec(const CSerialObject& obj, bool web);
        Spec(*self, web);
    }

//     void TextView() {
//         ncbi::CNcbiOstrstream buf;
//         buf << MSerial_AsnText << *self;
//         ncbi::CTextEditor::DisplayText(ncbi::CNcbiOstrstreamToString(buf));
//     }
};

#endif
