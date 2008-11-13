oligoFAR 3.28                     15-NOV-2008                                1-NCBI

NAME 
    oligoFAR version 3.28 - global alignment of single or paired short reads

SYNOPSIS
        usage: [-hV] [--help[=full|brief|extended]] [-U version]
          [-i inputfile] [-d genomedb] [-b snpdb] [-g guidefile] [-l gilist]
          [-1 solexa1] [-2 solexa2] [-q 0|1] [-0 qbase] [-c +|-] [-o output]
          [-O -eumxtadh] [-B batchsz] [-x 0|1|2] [-s 1|2|3] [-k skipPos]
          [--pass0] [-w win[/word]] [-N wcnt] [-n mism] [-e gaps] [-S stride] [-H bits]
          [--pass1  [-w win[/word]] [-N wcnt] [-n mism] [-e gaps] [-S stride] [-H bits] ]
          [-r f|s] [-a maxamb] [-A maxamb] [-y seqid [-y ...]] [-P phrap] [-F dust]
          [-p cutoff] [-u topcnt] [-t toppct] [-X xdropoff] [-I idscore]
          [-M mismscore] [-G gapcore] [-Q gapextscore] [-D minPair[-maxPair]]
          [-m margin] [-R geometry] [-L memlimit] [-T +|-]

EXAMPLES
    oligofar -U 3.28 -C human-data.ini -C deep-search.ini -i my.reads -h

    oligofar -i pairs.tbl -d contigs.fa -b snpdb.bdb -l gilist -g pairs.guide \
             -w 20/12 -B 250000 -H32 -n2 -rf -p90 -D100-500 -m50 -Rp \
             -L16G -o output -Omx

CHANGES
    Following parameters are new, have changed or have disappeared 
    in version 3.25: -n, -w, -N, -S, -x, -f, -R 
    in version 3.26: -n, -w, -N, -z, -Z, -D, -m, -S, -x, -f, -k
    in version 3.27: -n, -w, -e, -H, -S, -a, -A, --pass0, --pass1
    in version 3.28: -y, -R, -N

DESCRIPTION
    Performs global alignments of multiple single or paired short reads 
    with noticeable error rate to a genome or to a set of transcripts 
    provided in a blast-db or a fasta file.

    Reads may be provided as UIPACna base calls, possibly accompanied 
    with phrap scores (referred below as 1-channel quality scores), 
    or as 4-channel Solexa scores. Input file format is described 
    below in section FILE FORMATS.

    Output of srsearch (referred below as guide-file) or of a similar 
    program which performs exact or nealy exact short read alignment
    may be used as input for oligoFAR to ignore processing of perfectly 
    matched reads, but format the matches to output in uniform with
    oligoFAR matches way.

    Input is processed by batches of size controlled by option -B. 
    Reads to match are hashed (one window per read, preferrably at the 5' end)
    with a window size controlled by option -w. Option -n controls how many 
    mismatches are allowed within hashed values.  Option -a controls how many 
    ambiguous bases withing a window of a read may be hashed independently 
    to mismatches allowed. Low quality 3' ends of the reads may be clipped. 
    Low complexity (controlled by -F argument) and low quality reads may be 
    ignored.

    OligoFAR may use different implementations of the hash table (see -H):
    vector (uses a lot of memory, but is faster for big batches) and
    arraymap (lower memory requirements for smaller batches). 
    For vector -L should always be used and set to large value (GygaBytes).

    Database is scanned. If database is provided as blastdb, it is 
    possible to limit scan to a number of gis with option -l. If snpdb is
    provided, all variants of alleles are used to compute hash values, as well
    as regular IUPACna ambiguities of the sequences in database. Option -A
    controls maximum number of ambiguities in the same window.

    Alignments are seeded by hash and may be extended from 5' to 3' end by 
    one of the three algorithms:
        - simple score computation, no indels allowed (use -X0 for it);
        - Smith-Watermann banded alignment (use -Xn -rs for it, n > 0);
        - fast linear-time alignment (use -Xn -rf for it, n > 0), is almost as
          fast as no-indel algorithm, and times faster then the Smith-Watermann, 
          allows indels, but sometimes may produce suboptimal results.

    Alignments are filtered (see -p option). For paired reads geometrical
    constraints are applied (reads of the same pair should be mutually 
    oriented according to -R option, distance is set by -D and -m
    options). Then hits ranked by score (hits of the same score have same
    rank, best hits have rank 0). Week hits or too repetitive hits are 
    thrown away (see -t and -u options).

    At the end of each batch both alignments produced by oligoFAR and
    alignments imported from guide-file which have passed filtering and
    ranking get printed to output file (if set) or stdout (see FILE FORMATS
    for output format).

NOTE
    Since it is global alignment tool, independent runs against, say,
    individual chromosomes and run against full genome will produce different 
    results.

    To save disk space and computational resources, oligoFAR ranks hits by
    score and reports only the best hits and ties to the best hits. 
    In the two-pass mode tie hits may be incompletely reported - in this 
    case only hits of same score as the best are guarranteed to appear in 
    output no matter what value of -t is set.

    Scores of hits reported are in percent to the best score theoretically
    possible for the reads. Scores of paired hits are sums of individual
    scores, so they may be as high as 200.
    
PAIRED READS
    Pairs are looked-up constrained by following requirements: 
     - relative orientation (geometry) which may be set by --geometry or -R 
       (see section OPTIONS subsection ``Filtering and ranking options'')
     - distance between lowest position of plus-stranded read and highest
       position on minus-stranded one should be in range [ $a - $m ; $b + $m ] 
       where $a, $b and $m are arguments of parameters -D $a-$b -m $m.

    If pair has no hits which comply constraints mentioned above, individual 
    hits for the pair components still will be reported.

    Paired reads have one ID per pair. Individual reads in this case do not
    have individual ID, although report provides info which component(s) of 
    the pair produce the hit.

MULTIPASS MODE
    By default oligoFAR aligns all reads just once, but if option --pass1 is 
    used, oligoFAR switches to the two-pass mode. Parameters -w, -n, -e, -H 
    and -S preceeding --pass1 or following --pass0 affect first run, same 
    parameters when follow --pass1 are for the second run.  For the second run 
    only reads (or pairs) having more mismatches or indels then allowed in 
    parameters for the first pass will be hashed and aligned. So using something 
    like:

    oligofar --pass0 -w22/22 -n0 -e0 --pass1 -w22/13 -n2 -e1 

    will pick up exact matches first, and then run search with less strict 
    parameters only for those reads which did not have exact hits.

WINDOW, STRIDE AND WORD
    When hashing, oligoFAR first chooses some region on the read (called window).
    If the stride is greater then 1, it extracts the "stride" windows with offset 
    of 1 to each other. Then each window gets variated (with mismatches and/or 
    indels). If word size is equal to window size, window is hashed as is. If 
    word size is smaller then window, two possibly overlapping words are created
    and added to hash: at the beginning of the window and at the end of the 
    window.

    Example:
    
    +-------------+  hashed region (should not exceed 32 bases)
    ACGTGTTGATGACTActgatgatctgat
    +-----------+    window size = 13
    ACGTGTTGATGAC    window 1 \
     CGTGTTGATGACT   window 2  } stride = 3
      GTGTTGATGACTA  window 3 /
    +-------+        word size = 9
    ACGTGTTGA        word 1 of window 1
        GTTGATGAC    word 2 of window 1
     CGTGTTGAT       word 1 of window 2
         TTGATGACT   word 2 of window 2
      GTGTTGATG      word 1 of window 3
          TGATGACTA  word 2 of window 3

    If you allow indels to be hashed, hashed region is extended by 1 base. So 
    for window of 24 and stride 4 with indels allowed reads should be at least
    28 bases long (27 with no indels).

    Words when hashing are splet in the two parts: index and supplement. 
    Supplement can't have more then 16 bits. Index can't be more then 31bits.
    Therefore maximal word size should fit in 47 bits and is 23 bases.

OPTIONS

Service options
    --help=[brief|full|extended]
    -h          Print help to stdout, finish parsing commandline, and then 
                exit with error code 0. Long version may accept otional 
                argument which specifies should be printed brief help version
                (synopsis), full version (without extended options) or extended
                options help.

                The output (except brief) contains current values taken for options, 
                so commandline arguments values which preceed -h or --help will
                be reflected in the output, for others default values will be
                printed.

    --version
    -V          Print current version and hash implementation, finish parsing
                commandline and exit with error code 0.

    --assert-version=version
    -U          If oligofar version is not that is specified in argument,
                forces oligofar to exit with error.  Every time this option 
                appears in commandline or in config file, the comparison is
                performed, so each config file may contain this check
                independently.  

    --test-suite=+|-
    -T +|-      Run internal tests for basic operations before doing anything
                else.

    --memory-limit=value
    -L value    Set upper memory limit to given value (in bytes). Suffixes k,
                M, G are allowed. Default is 'all free RAM + cache + buffers'.
                Recommended value for VectorTable hash implementation is 14G or
                above.

File options
    --input-file=filename
    -i filename  Read short reads from this file. See format description in
                section FILE FORMATS.

    --fasta-file=filename
    -d filename Sets database file name. If there exists file with given name
                extended with one of suffixes .nin, .nal - the suffixed version 
                is opened as blastdb, otherwise the file itself is expected to
                contain sequences in FASTA format. 
    
    --guide-file=filename
    -g filename Sets guide file name. The file should have exactly the same order 
                of reads as input file does. There may be multiple hits per each
                read, or some reads may be skipped.

    --snpdb-file=filename
    -b filename Sets SNP database filename. Snpdb is a file prepared by an
                oligofar.snpdb program.

    --output-file=filename
    -o filename Sets output filename.

    --gi-list=filename
    -l filename Sets file with list of gis to which scan of database should be
                limited. Works only if database is in blastdb.

    --only-seqid=seqid
    -y seqid    Limits database lookup to this seqid. May appear multiple
                times - then list of seqids is used. Does not work with -l.
                Comparison is pretty strict, so lcl| or .2 are required in
                'lcl|chr12' and 'NM_012345.2'. 

    --qual-1-file=filename
    -1 filename Sets file with 4-channel scores for the first component of
                paired reads or for single reads. Should contain data in
                exactly same order as the input file.

    --qual-2-file=filename
    -2 filename Sets file with 4-channel scores for the second component of
                paired reads. Should contain data in exactly same order an the
                input file.

    --quality-channels=cnt
    -q 0|1      Affects expected input file format - indicates does it have
                1-channel scores for reads in additional (4th and 5th ) columns.

    --quality-base=value
    --quality-base=+char
    -0 value
    -0 +char    Sets base value for character-encoded phrap quality scores,
                i.e. integer ASCII value or `+' followed by character which 
                corresponds to the phrap score of 0.

    --guide-max-mism=count
    -x count    Maximal number of mismatches for hits in guidefile to be used.

    --colorspace=+|-
    -c +|-      Input is in di-base colorspace encoding. Hashing and alignment 
                will be performed in the colorspace encoding. 
                Not compatible with -q, -1, -2 parameters.

    --output-flags=-eumxtadh
    -O-eumxtadh Controls what types of records should be produced on output.
                See OUTPUT FORMAT section below. Flags stand for:
                e - empty line
                u - unmapped reads
                m - "more" lines (there were more hits of weaker rank, dropped)
                x - "many" lines (there were more hits of this rank and below)
                t - terminator (there were no more hits except reported)
                a - alignment - output alignment details in comments 
                d - differences - output positions of misalignments
                h - print all hits above threshold before ranking
                Use '-' flag to reset flags to none. So, -Oeumx -Ot-a is
                equivalent to -O-a.

    --batch-size=count
    -B count    Sets batch size (in count of reads). Too large batch size
                takes too much memory and may lead to excessive paging, too
                small makes scan inefficient. 

Hashing and scanning options
    --pass0     Following -w, -n, -e, -S and -H options will apply to first
                pass.

    --pass1     Turns on two-pass mode; following -w, -n, -e, -S and -H 
                options will apply to the second pass.

    --window-size=window[/word]
    -w window[/word]
                Sets window and word size.

    --max-windows=count
    -N count    Set maximal number of consecutive windows to hash.
                Should be 1 (default) if used with -k. Actual number of words
                will be multiplied by stride size. Also alternatives, indels 
                and mismatches will extend this number independently.

    --input-mism=mism
    -n mism     Sets maximal allowed number of mismatches within hashed word.
                Reasonable values are 0 and 1, sometimes 2.

    --input-gaps=[0,]0|1
    -e [0,]0|1  Sets maximal number of indels per window. Allowed values are 
                0 and 1.

    --index-bits=bits
    -H bits     Set number of bits in hash value to be used as an direct index.
                The larger this value the more memory is used.

    --stride-size=size
    -S stride   Stride size. Minimum value is 1. Maximum is one base smaller 
                then word size. The larger this value is, the more hash 
                entries are created, but the less hash lookups are performed
                (every "stride's" base of subject). 

    --window-skip=pos,...
    -k pos,...  Always skip certain positions of reads when hashing.
                Useful in combination with -n0 when it is known that certain
                bases have poor quality for all reads.
                If used with --ambiguify-positions=true (extended option, may
                be changed in next release), just replaces appropriate bases
                with `N's, then this option affects scoring as well.

    --input-max-amb=count
    -a count    Maximal number of ambiguous bases in a window allowed for read 
                to be hashed. If oligoFAR fails to find window containing 
                this or lower number of ambiguities for a read, the read is 
                ignored. 

    --fasta-max-amb=count
    -A count    Maximal number of ambiguous bases in a window on a subject
                side for the window to be used to perform hash lookup and 
                seed an alignment. 

    --phrap-cutoff=score
    -P score    Phrap score for which and below bases are considered as
                ambiguous (used for hashing). The larger it is, the more
                chances to seed lower quality read to where it belongs, but
                also the larger hash table is which decreases performance. 
                Affects 1-channel and 4-channel input. Replaces obsolete 
                --solexa-sencitivity (-y) option.

    --max-simplicity=dust
    -F dust     Maximal dust score of a window for read to be hashed or for
                the hash lookup to be performed. 

    --strands=1|2|3
    -s 1|2|3    Strands of database to scan. 1 - positive only, 2 - negative
                only, 3 - both.

Alignment options
    --algorithm=f|s
    -r f|s      Alignment algorithm to use if indels are allowed:
                s - Smith-Watermann (banded)
                f - Fast linear time

    --x-dropoff=value
    -X value    X-dropoff. 0 forbids indels, otherwise for banded Smith-Watermann 
                it controls band width. Does not affect fast algorithm if
                above 0.

    --identity-score=score
    -I score    Set identity score

    --mismatch-score=score
    -M score    Set mismatch score

    --gap-opening-score=score
    -G score    Gap opening score

    --gap-extention-score=score
    -Q score    Gap extention score

Filtering and ranking options
    --min-pctid=pct
    -p pct      Set minimal score for hit to appear in output.  Scores reported are 
                in percent to the best score theoretically possible for the read, so 
                perfect match is always 100.

    --top-percent=pct
    -t pct      Set `tie-hit' cutoff. Taking the best hit for the read or read
                pair as 100%, this is the weakest hit for the read to be
                reported. So, if one has -t 90, and the best hit for some read
                has score 90, report will contain hits with scores between
                90%*90 = 81 and 90 (provided that -p is 81 or below). This
                option does not affect lines generated by output option -Oh.

    --top-count=cnt
    -u cnt      Set maximal number of hits per read to be reported. The hits
                reported by ouptut option -Oh do not count - they will be
                reported all.

    --pair-distance=min[-max]
    -D m[-M]    Set allowed range for distance between component hits for 
                paired read hits. If max is omited it is considered to be equal 
                to min.Length of hits should be included to this length. 

    --pair-margin=bases
    -m bases    Set fuzz, or margin for `insert' length for paired reads. The
                actual range of insert lengths allowed is [a - m, b + m],
                where a, b, and m are set with -D a-b -m m command line parameters.

    --geometry=type
    -R geometry Sets allowed mutual orientation of the hits in paired read hits. 
                Values allowed (synonyms are separated by `|') are:

        p|centripetal|inside|pcr|solexa     reads are oriented so that vectors 
                                            5'->3' are pointing to each other
                                            ex: >>>>>>>   <<<<<<<

        f|centrifugal|outside               reads are oriented so that vectors 
                                            5'->3' are pointing outside
                                            ex: <<<<<<<   >>>>>>>

        i|incr|incremental|solid            reads are on same strand, first 
                                            preceeds second on this strand
                                            ex: >>>1>>>   >>>2>>>
                                            or: <<<2<<<   <<<1<<<

        d|decr|decremental                  reads are on same strand, first 
                                            succeeds second on this strand
                                            ex: >>>2>>>   >>>1>>>
                                            or: <<<1<<<   <<<2<<<

                In examples above the pattern >>>1>>> means first component of 
                the paired read on plus strand, <<<2<<< means second component 
                on reverse complement strand; if the digit is not set then the
                component number does not matter for the example.

                Combinations of the values are not allowed.

Extended options
    These options are supposed more for development cycle - they may choose 
    development versus production algorithm implementations, or set parameters 
    that may not supposed to be exposed in the future. These options should 
    not be used on production and are not guarranteed to be preserved.

    --min-block-length=bases
                Set minimal length of the subject sequence block to be 
                processed as a whole with same algorithm (depending on 
                presence of ambiguity characters).  Added to tune 
                performance when high density of SNPs is provided. Makes
                no difference with -A1.

FILE FORMATS

Input file
    Input file is two to five column whitespace-separated text file. Empty
    lines and lines starting with `#' are ignored (note: they are ignored as
    if not present, so if you use # to comment out a read but provide solexa
    file with -1, or if you use guide file, the corespondense of read ids to 
    the scores or to gided hits will be broken).

    Columns:
    #1 - considered as read-id or read-pair id
    #2 - UIPACna sequence of read #1
    #3 - UIPACna sequence of read #2 (optional), or '-' if the read does not have pair
    #4 - quality scores for read #1 or '-' (optional)
    #5 - quality scores for read #2 or '-' (optional)

    Quality scores should be represented as ASCII-strings of length equal to 
    appropriate read length, one char per base, with ASCII-value of each char 
    minus 33 representing phrap quality score for the appropriate base.  The 
    number 33 here may be changed with -0 (--quality-base) parameter.

    If -q1 is used, column 4 is required and should not be '-'; same as column 5 
    if column 3 is not '-' and not empty.

    If -q0 is used, columns 3-5 are optional and columns 4, 5 are never used.
    Column 3 is used if present and non-empty and not '-'.

    Mixed (paired and non-paired) input is allowed - rows with columns 3,5 
    containing '-' may interleave rows with all four columns set.

    Example:

    rd1    ACAGTAGCGATGATGATGATGATGATWNG    -    ????>????=<?????>>>(876555+!.  -

    Here in column 4 each char represents a base score for appropriate base in
    column 2, e.g. ? indicates phrap score of 30, > stands for phrap score of 
    29, etc.

Input file with di-base colorspace reads (SOLiD technology)
    Reads may be specified in di-base colorspace encoding.  Option --colorspace=+ 
    should be used, quality scores will be ignored.  SEquence representation should 
    be following: first base is IUPACna, all the rest are digits 0-3 representing 
    dibases:

    0 - AA, CC, GG, or TT
    1 - AC, CA, GT, or TG
    2 - AG, GA, CT, or TC
    3 - AT, TA, CG, or GC

    Example: 

    rd1    C02033003022113110030030211    -
    
    The read above represents sequence CCTTATTTAAGACATGTTTAAATTCAC.

Guide file
    Guide file is an output of srsearch program or similar tool which finds 
    exact or almost exact matches for reads and read pairs.

    It should be tab-separated file having records with 7 or 11 columns (11 for
    paired hits or paired reads) with columns:

    1. type of result:
       for non-paired search - always 0;
       for paired search: 
        0 - paired match; 
        1 - non-paired match for the first member of the pair; 
        2 - non-paired match for the second member of the pair;
    2. query ordinal number (0-based) or query id;
    3. subject ordinal number (OID) or subject id;
    4. subject offset 1-based (for the first member of the pair if paired match);
    5. '0' or '-' - reverse strand; '1' or '+' - forward strand (for the first 
       member of the pair if paired match);
    6. comma-separated list of 1-based mismatch positions for the first
       member of the pair if paired match; 0 values are ignored (so single 0 
       is an empty list); dash (-) is considered for empty list as well; 
    7. subject base at mismatch position ('-' for exact match) (for the
       first member of the pair if paired match);
    8. if paired match - subject offset of the second member of the pair;
    9. if paired match - strand of the second member of the pair;
    10. if paired match - mismatch position(s) of the second member of the
        pair, like col 6;
    11. if paired match - subject base at mismatch position for the second
        member of the pair.

    It is essential that order of hits in guide is the same as order of reads in
    input file.

    When parsing input, oligoFAR reads guide file and ignores records which do not
    satisfy following constraints:

    1. Column 1 should be 0, as well as columns 6 and 10 (if exists)
    2. If record has 11 columns, column 5 should differ from column 9
    3. If column 5 = '+' or '1', then column 8 should be > then column 4,
       otherwise column 4 should be > then column 8
    4. Distance between values in columns 4 and 8 plus length of read should
       fit within allowed distance (between min - margin and max + margin)

    If read is not ignored, it is added to output list, and read or read pair will
    bypass alignment procedure.

Gi list file
    is just list of integers, one number per line.
    
Solexa-style score file
    Should contain lines of whitespace-separated integer numbers, one line per
    read, 4 numbers per base. Order of reads in the file should correspond to
    order or reads in input file. Input file is required - it provides read
    IDs. If solexa-style file is set, IUPACna and quality scores from input
    file are ignored.

OUTPUT FORMAT
    Output file is a 15-column tab-separated file representing different types
    of records (see -O options) in uniform way. 

    Columns:
    1.  Rank of the hit by score (0 - highest), 
        or `*' for unranked hits (see -Oh).
    2.  Number of hits of this rank, or "many" if there are more then it
        can be printed by -u option value, or "none" if there are no hits for 
        the query line, or "more" if followed after line "many", or "no_more"
        for terminator lines (see -Ot), or "hit" for unranked hits (see -Oh),
        or "diff" for lines that present differences of reads to subject 
        (see -Od). See details below.
    3.  Query-ID - read or read pair ID.
    4.  Subject-ID - one or more IDs of the subject sequence. Which one(s) is 
        not defined here and is implementation dependent.
    5.  Mate bitmask: 0 for no hits, 1 if first read matches, 2 if second
        read matches, 3 for paired match.
    6.  Total score for the hit (sum or individual read scores)
    7.  Position `from' on subject for the read or for the first read of the 
        pair for paired read, or `-' if it is hit for the second read of the 
        pair only.
    8.  Position `to' for the read or for the first read of the paired read,
        or `-' if it is hit for the second read of the pair. This position is
        included in the range, and indicates strand (to < from for negatsve 
        strand). 
    9.  Score for first read hit, or `-'.
    10. Position `from' for the second read, or `-'.
    11. Position `to' for the second read (to < from for negative strand), 
        or `-'.
    12. Score for second read hit, or `-'.
    13. Pair "orientation": '+' if first read is on positive strand, '-'
        if second read is on positive strand.
    14. Instantiated as IUPACna image of positive strand of the subject 
        sequence at positions where the first read maps, or '-'.
    15. Instantiated as IUPACna image of positive strand of the subject 
        sequence at positions where the second read maps, or '-'.

    "Ideal" mapping (having single best) should consider using filter on first 
    column = 0, second column = 1

    Scores for individual reads have maximum of 100, for paired - 200. No
    reads with score below the one set with -p option may appear in output.
    Since filtering by -p is performed before combining individual hits to
    paires, value for -p should not ever exceed 100. 

    If the flag a for -O is set, for every individual read of hit which score 
    is below 100 three additional lines will be printed. These lines start
    with `#' and contain graphical representation of the alignment (in subject
    coordinates, so that query may appear as reverse complement of the
    original read):

    # 3'=TTTCCTTTAGA-AGAGCAGATGTTAAACACCCTTTT=5' query[1] has score 68.6
    #     |||||||||| ||||||| | | ||||||||||||    i:31, m:4, g:1
    # 5'=ATTCCTTTAGATAGAGCAGTTTTGAAACACCCTTTT=3' subject

    or for di-base colorspace reads:

    # 3'=31200000222133320222333030T=5' query[1]
    #    ||| |||||||||||||||||||||||    i:26, m:0, g:1
    # 5'=TAC-TTTTTCTCATATCCTCTATAATT=3' subject

    This format is intended for human review and may be changed in future
    versions.

Output record types
    If a flag `h' was used for -O option, every individual hit with score above
    that was set by -p option will be immediately reported in a line looking
    like

    *    hit    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?

    where ? are set to appropriate values. If the hit bypasses ranking it will
    be also printed as ranked hit after batch is processed. Regular ranked
    hits output may look like:

    0  1     rd1  gi|192  1  100  349799  349825  100  - - - +  ACT...  -
    1  3     rd1  gi|195  1  95   799070  799096  95   - - - +  AGT...  -
    1  3     rd1  gi|198  1  95   99070   99096   95   - - - +  ACT...  -
    1  3     rd1  gi|298  1  95   299050  299024  95   - - - -  TTA...  -
    2  many  rd1  gi|978  1  92   267050  267076  92   - - - -  TTA...  -
    2  many  rd1  gi|576  1  92   167050  167076  92   - - - +  ACT...  -
    2  more  rd1  *       *  *    *       *       *    * * * *  *       *

    Line 1 here means that there is 1 (col 2) hit with best score (col 1) of 
    100 (col 6).

    Lines 2-4 mean that there are also three (col 2) hits with next to best 
    (col 1) score (col 6) of 95.

    Lines 5,6 mean that there are many (col 2) reads of rank 2 in score (if 
    flag x in -O is not used, there will be number 2 in col 2 of these lines).

    Line 7 means that there are more hits of rank 2 and below, which are not
    reported. If flag m in -O is not set, this line will not appear).

    This example suggests that -u 6 option was used (that's why no more hits
    are reported).

    If all hits with score above that given by -p are reported, output would
    look like:

    0  1       rd1 gi|192  1  100  349799  349825  100  - - - +  ACT...  -
    1  3       rd1 gi|195  1  95   799070  799096  95   - - - +  AGT...  -
    1  3       rd1 gi|198  1  95   99070   99096   95   - - - +  ACT...  -
    1  3       rd1 gi|298  1  95   299050  299024  95   - - - -  TTA...  -
    2  3       rd1 gi|978  1  92   267050  267076  92   - - - -  TTA...  -
    2  3       rd1 gi|576  1  92   167050  167076  92   - - - +  ACT...  -
    2  3       rd1 gi|585  1  92   465010  465036  92   - - - +  ACT...  -
    2  no_more rd1 -       0  0    -       -       0    - - - *  -       -

    Last line appears only if the flag t of option -O is used.

    If the flag d of option -O is set, every record having score below 100 (or
    below 200 for paired reads) will be followed by one or more records of
    type `diff':

    1  diff  rd1  gi|195  1  95  799071  799071  95  -  -  -  +  C=>G  -

    which means that base C at position 799071 of gi|195 is replaced with G in
    read. Diff lines are always converted to be on positive strand of the
    subject sequence.  Differences longer then one base may be reported,
    IUPACna with ambiguity characters extended with `-' for deletion may be
    used. For paired reads differences for each read are reported in separate
    records, but in appropriate columns (for second read columns 10, 11, 12, 15
    are used instead of 7, 8, 9, 14). Column 13 is always `+'.

    If the flag u of option -O is set, and read has no matches, the line like
    following will appear on output:

    0  none  rd2  -  0  0  -  -  0  -  -  0  *  ACG... -

    where columns 14 and 15 will contain basecalls of the read or reads (if it
    is pair) instead of subject sequence interval.

    Normally all records except `hit' record are clustered by read id (but not
    sorted). If flag e of option -O is set, an empty line is inserted in
    output between blocks of records for different reads to visually separate
    them.

BUGS, UNTESTED AND DEVELOPMENT CODE, SPECIAL CASES
    - Mutual orientation restriction code is not tested on guide files yet

    - Fast alignment code produces suboptimal alignments by design. Some patterns
      may be improved in time, but to gain speed it gives up pricision. One of 
      the typical cases when it currently makes mistakes is pattern 'flip+indel' 
      like:
            TTA-CC      TT-ACC
            ||  ||      || |||
            TTCACC      TTCACC
      which produces mismatch + indel instead of indel + match

    - Di-nucleotide colorspace is at development stage - scoring is very
      trivial and suites for mapping, not for SNP detection.

EXIT VALUES
    0 for success, non-zero for failure.

END

vim:expandtab:tabstop=4
