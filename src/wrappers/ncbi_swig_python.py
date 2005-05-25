import sys, os, re
import sppp

# read input file and make swig interface file

ifname = sys.argv[1]
swig_input_fname = os.path.splitext(ifname)[0] + '.i'

outf = open(swig_input_fname, 'w')
headers = sppp.ProcessFile(ifname, outf)
outf.close()

# Run swig preprocessor
cline = '''
%s -E -python -c++ -modern -importall -ignoremissing \
-w201,312,350,351,361,362,383,394,401,503,504,508,510 \
-I./dummy_headers -I./python \
-I%s -I%s \
%s > %s \
''' % (os.environ['SWIG'], os.environ['NCBI_INC'], os.environ['NCBI_INCLUDE'],
       swig_input_fname, swig_input_fname + '.pp')
status = os.system(cline)
if status:
    sys.exit(status)


# Modify preprocessor output
fid = open(swig_input_fname + '.pp')
s = fid.read()
fid.close()
lines = s.split('\n')
mod_pp = open(swig_input_fname + '.mpp', 'w')
for line in lines:
    if line.startswith('%importfile'):
        m = re.search('%importfile "(.*)"', line)
        fname = m.groups()[0]
        for header in headers:
            if fname.endswith(header):
                line = line.replace('%importfile', '%includefile', 1)
                break
    mod_pp.write(line + '\n')
mod_pp.close()

# Run swig on modified preprocessor output
ofname = os.path.splitext(ifname)[0] + '_wrap.cpp'
cline = '''
%s -o %s -nopreprocess \
-python -v -c++ -modern -importall -ignoremissing -noextern \
-w201,312,350,351,361,362,383,394,401,503,504,508,510 \
-I./dummy_headers -I./python \
-I%s -I%s \
%s \
''' % (os.environ['SWIG'], ofname, os.environ['NCBI_INC'],
       os.environ['NCBI_INCLUDE'],
       swig_input_fname + '.mpp')
status = os.system(cline)
if status:
    sys.exit(status)
