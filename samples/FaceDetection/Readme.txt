Description
===========

This is an abstraction of a cascade face detector used in the computer vision
community. It includes a cascade of classifiers. The first classifier, called
(classifier1) operates on images. It is controlled by a tag collection,
called <classifier1_tags>. This tag collection contains tags specifying the
image. If the first classifier determines that an image might be a face, it
produces that same tag value but into a different tag collection,
<classifier2_tags> that controls the second classifier. This tag collection 
will be a subset of the first tag collection. An image may fail on any
classifier but if it makes its way to the end, it is deemed to be a face.
If an image passes the last of the classifiers, it is identified in the item
collection [face]. 

In the real cascade face detector, the classifiers are determined by machine
learning, but you might think of them as looking for eyes, nose, mouth, etc.
Also the real face detector examines not only whole images but many overlapping
subimages of the same image. 

The constraints on parallelism for this application are that an instance of
(classifier2) can not execute until the associated instance of (classifier1)
has executed and determined that it might be a face. Similarly an instance of
(classifier3) has a control dependence on (classifier2). Instances of a given
classifier can execute at the same time and instances of distinct classifiers
can execute at the same time as long as the earlier constraint is met.


Usage
=====

The command line is:

FaceDetection [-v] number_of_images
    -v : verbose option
    number_of_images : a positive integer for the number_of_images
                       to be randomly generated
    
e.g.
FaceDetection -v 20

The output prints the number of faces detected.
If -v is specified, the output also prints the input images and
the faces found.


