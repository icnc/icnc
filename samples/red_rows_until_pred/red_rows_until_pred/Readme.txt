This program corresponds to the pattern 
   where the steps are within the same collection
   there is a producer/consumer relation
   there is a controller/controllee relation

This program inputs a set of input items tagged by inputID. For each input,
it applies a function f to create a new value for inputID. The different
values computed for the same inputID are distinguished by a second tag component
called current.  When predicate p is true of the value, the processing for that
inputID is complete. The value when predicate p is true is stored in [result].
[result] only distinguishes among distinct inputID values. The iteration on
which the predicate became true is no longer needed. [result] is emitted to
the environment.

The [dataValues] contains a struct that contains the two fields, the latest
value, the running sum.

Function f uses the current value as a seed to random number generator to
produce the next value. 

The predicate p succeeds when the sum is greater than some number.

[results] indicates the value that passed the predicate test.

The one constraint on parallelism is that the running sum for each inputID
must be processed in order. There are no ordering requirements among the
different inputIDs. They can be processed in parallel.


Usage
=====

The values of inputID are from 1 to INPUTID_COUNT.  The sum limit is MAXSUM.
#define MAXSUM 200
#define INPUTID_COUNT 5

The program prints the input and output of (process) step instances and
the values of [results] instances.
