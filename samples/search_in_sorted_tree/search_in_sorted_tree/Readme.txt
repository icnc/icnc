This program corresponds to the pattern 
   where the steps are within the same collection
   there is no producer/consumer relation
   there is a controller/controllee relation

This program uses a sorted tree of ints.  The ints themselves are at the
leaves. the intermediate nodes contains the largest int in the left subtree
(the subtree with the smallest values). This value is called the partition
value. Each node in the tree is an item. The program inputs this tree as
[treeNode]. It also inputs a list of ints to search for. These input as
[searchVal]. For each int in the list the program searches the sorted tree
to see if the int is there. 

The process of searching for a single [searchVal] starts at the top of the
tree and at each intermediate node processes the right or left branch of the
tree depending on the relationship between the partition value in the tree
node and the searchVal.

The tree nodes are identified by an int containing only zeros and ones.
The root is identified as one. zero indicates the left child, one indicates
the right child. Node 1011 is the right child of the right child of the left
child of the root.

In the code below, tags called node, correspond to a node in the
search tree and tag components called i correspond to the position in
the list of inputs to search for (not the value searched for).


Usage
=====

A simple sorted tree and the search values are initialized in main.

At the end of search, the program iterates over <success> and
prints the values found and their associated tree nodes.
