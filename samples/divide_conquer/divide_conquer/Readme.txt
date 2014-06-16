Description
===========

This application uses an encoding of integer tags to represent nodes in trees.
The tags are decimals of zeros and ones. Each tag represents a path from the
root to the node where 0 is the left child of the parent and 1 is the right
parent. The root tag is 1.  So the tag 101 is the right child of the left child
of the root.

The step collection (divide) inputs the root instance of the item collection
[divideItem]. It creates the whole [divideItem] collection. When a (divide)
instance determines that it is at the bottom of the [divideItem] tree, it
transfers the [divideItem] contents to the leaves of the [conquerItem] tree.
The (conquer) step collection, creates the [conquerItem] tree bottom-up
eventually producing its root.

Each (divide) or (conquer) step instance produces new [divideItem] or
[conquerItem] instances to be processed. It also produces new <divideTag>
or <conquerTag> instances to control that processing. For (divide), the
[divideItem] instances and their associated <divideTag> instances have the
same tag values. For example, if a step produces [divideItem: 101] it also
produces <divideTag: 101> to process that item. On the other hand, a (conquer)
with tag 101 gets both [conquerItem: 1010] and [conquerItem: 1011], so one of
the two instances of (conquer) that produce these two [conquerItem] instances,
produces the tag <conquerTag: 101>. We arbitrarily assign this task to the left
child.

The constraints on parallelism for this application are as follows:
a [divideItem] instance may not be processed by a (divide) instance until
the [divideItem] instance and the associated controlling tag <divideTag>
has been produced. Note that the item and the tag are produced by the same
step instance. Similarly, a [conquerItem] instance may not be processed by a
(conquer) instance until that [conquerItem], its sibling item and the
controlling <conquerTag> have been produced. These three instances are
produced by two distinct instances of (conquer). These are the only
constraints. This allow for execution of all the (divide) instances before
any of the conquer instances. It allows for breadth-first or depth-first
processing within each collection. It also allows for depth-first processing
of (divide) allowing (conquer) processing to start before (divide) is complete.
These are scheduling possibilities for this graph specification.


Usage
=====

User input for the value of the root node is specified by
	const int INPUT_CONTENTS = 18;
in function main.  The value can be changed to any positive integer.

The output shows a trace of the divide and conquer steps.  At the end of
the execution the final conquer item output to the environment should be
the same as the initial user input.
