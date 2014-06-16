Description
===========

This example is a producer-consumer program.  there are two step collections,
one called (producer) and one called (consumer). (producer) and (consumer)
step collections are both prescribed by the same tag collection, called
<prodConsTag>. This means that for each tag instance in that collection,
there will be an instance of (producer) and an instance of (consumer) with
that same tag. The tag collection <prodConsTag> is produced by the environment.

Instances in the (producer) step collection produce item instances in the
[valueItem] collection which are consumed by instances in the (consumer)
collection.

[inItem] and [outItem] are included to create a full program. They are input
to (producer) and output from (consumer) respectively. 

The only constraint on parallelism in this program is that for a given instance
of [valueItem] it must be produced before it is consumed. This implies that
distinct instances of (producer) may execute at the same time, distinct
instances of (consumer) may execute at the same time and unrelated instances
of (producer) and (consumer) may execute at the same time.


Usage
=====

User input for the number of instances of (producer) and (consumer) to be
executed is specified by
	const int N = 10;
in function main.  The value can be changed to any positive integer.

The program prints the value of outItem instances on the console.

