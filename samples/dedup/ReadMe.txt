parsec/dedup
============
From http://parsec.cs.princeton.edu/doc/parsec-report.pdf:

"The dedup kernel was developed by Princeton University.  It compresses
a data stream with a combination of global compression and local
compression in order to achieve high compression ratios. Such a
compression is called 'deduplication'.  The reason for the inclusion
of this kernel is that deduplication has become a mainstream method to
compress storage footprints for new-generation backup storage systems
and to compress communication data for newgeneration bandwidth
optimized networking appliances.

The kernel uses a pipelined programming model to parallelize the
compression to mimic real-world implementations.  There are five
pipeline stages the intermediate three of which are parallel. In the
first stage, dedup reads the input stream and breaks it up into
coarse-grained chunks to get independent work units for the
threads. The second stage anchors each chunk into fine-grained small
segments with rolling fingerprinting.  The third pipeline stage
computes a hash value for each data segment. The fourth stage
compresses each data segment with the Ziv-Lempel algorithm and builds
a global hash table that maps hash values to data. The final stage
assembles the deduplicated output stream consisting of hash values and
compressed data segments.

Anchoring is a method which identifies brief sequences in a data
stream that are identical with sufficiently high probability.  It uses
fast Rabin-Karp fingerprints to detect identity. The data is then
broken up into two separate blocks at the determined location. This
method ensures that fragmenting a data stream is unlikely to obscure
duplicate sequences since duplicates are identified on a block basis."

The CnC implementation makes steps of the stages of the pipline.


Notes
-----

The code requires zlib which is freely available at
http://www.zlib.net/
and openssl which can be downloaded from
http://slproweb.com/products/Win32OpenSSL.html.
Most Linux distributions come with system wide installations of
zlib and openssl, so nothing needs to be done. On Windows, the paths
to zlib and openssl need to be adjusted in the project file (according
to your installation).


Usage
-----
dedup [-c] [-i file] [-o file]
-c 			compress
-i file			the input file/src host
-o file			the output file/dst host
-h 			help
