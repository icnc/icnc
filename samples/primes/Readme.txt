Description
===========

Primes has a single step collection called (compute). It is executed once
for each tag in the tag collection <oddNums>. This tag collection is produced
by the environment.

(compute) does not input any items. It simply uses its tag and determines if
the tag value is a prime.  If the tag value is a prime, the instance of
(compute) emits an instance of an item collection called [primes].
The item instance has a value of the tag itself.

There are no constraints on the parallelism in this example. An instance of
(compute) does not rely on a data item from another instance and it does not
relay on a control tag from another instance. All the tags are produced by
the environment before the program begins execution. All steps can execute in
parallel if there are enough processors.

There are four different implementations: one serial and three using
CnC. primes demonstrates the straight-forward CnC implementation.
primes_range aims to adjust the grain-size by using a tag-range.
primes_parfor will behave similar but is much simpler by using
CnC::parallel_for. Please refer to the documentation for description
of these features. 


Usage
=====

The command line is:

primes [-v] n
    -v : verbose option
    n  : positive integer for upperbound of the range of prime computation
    
e.g.
primes -v 100

The output prints the number of primes found in the range 1-n.
If -v is specified, the output also prints the primes found.

