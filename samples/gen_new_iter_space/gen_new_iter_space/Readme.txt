This code corresponds to primitive pattern that
   has two distinct step codes, (generateIterations) and (executeIterations),
   has a controller/controllee relation between them
   has no producer/consumer relation between them


The (generateIterations) step executes once. It generates a collection of tags
that correspond to the integers from 1 to imax. The step (executeIterations)
executes once for each of these tags.

One might ask why the (generateIterations) collection which only has a single
instance is not simply performed in the environment. There are several answers
to this question:
 - It could well be part of the environment. That would be fine.

On the other hand if this fragment were part of a larger program, this approach
might make more sense.
 - For example, if some significant part of the computation results in the
   value of imax or input to (executeIterations), there is no need to go out
   to the environment and back again. The environment can contain serial
   code between parallel regions. 
 - As another example, if the tags were modified to include another tag
   component, then instead of a serial computation generating a one-dimensional
   computation, we would have a one-dimensional computation generating a
   two-dimensional computation. This approach would make more sense.


Usage
=====

User input for the number of iterations to generate is specified by
	const int numIters = 100;
in function main.  The value can be changed to any positive integer.

The program prints the value of result instances which are values from
1 to numIters in random order.
