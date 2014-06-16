This program corresponds to the pattern 
   where the steps are in distinct collections
   there is a producer/consumer relation
   there is a controller/controllee relation


This program inputs a string and generated a collection of strings that
partitions the input into subparts that contain the same character

e.g., "aaaffqqqmmmmmmm"
becomes

"aaa"
"ff"
"qqq"
"mmmmmmm"

Our example:

 - The [input] is a single string of char
 
 - [span] is a collection of strings of char

 - <singletonTag> a single tag instance used for createSpan

 - <spanTags> identifies the instances in the span collection

 - (createSpan) creates that span from the input

 - (processSpan) processes the created span
 
 - [results] is a collection of strings with odd-length only

One might ask why we execute the (createSpan) step, which only happens once,
within the graph and not in the environment. We could do this.
If this fragment were part of a larger program then in some situations it
might be better not to.
 - For example, if there were a collection of strings as input to be processed,
   there would be another tag component specifying the string.
 - Another example might be that the string to be processed is the output of
   significant earlier part of the program and it also had potential
   parallelism. There is no need to go out to the environment for the serial
   work and then back to the graph for each transition between
   serial and parallel parts of the application.

In this application as it stands, the single instance of (createSpan) must
occur first since it generates the [output] items to be processed. Then the
(processSpan) steps can execute in parallel.


Usage
=====

The single input string is "aaaffqqqmmmmmmm".

The program prints the input string and the instances of [results] which are
substrings of the input string which contain odd number of the same character.

