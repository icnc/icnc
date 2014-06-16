
This program corresponds to the pattern 
   where the steps are within the same collection
   there is a producer/consumer relation
   there is no controller/controllee relation

This program operated on a two dimensional collection tagged by row and col.
A collection of items are input to the program together with a corresponding
collection of tags. Within each row, the program processes the input items
in order across the columns and computes a running sum into [sum].

The constraints on parallelism in this example are that the columns in a given
row must be processed in order. The processing of different rows are totally
independent.


Usage
=====

The readOnlyItem collection has dimension (1..3, 1..5) and the instances are
intialized to (row*10 + col).

The program prints the value of the final sum for each row.
