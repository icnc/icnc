Description
===========

Stencil computation is the basis for the reverse time migration
algorithm in seismic computing.  The underlying mathematical problem
is to solve the wave equation using finite difference method. This
benchmark computes a 25-point 3D stencil. 

The CnC implementations both use explicit tiling. For better
distributed performance the second version also uses explicit halo
objects on the boundaries. Both algorithm behaves highly asynchronously
even when the halos are updated after every time step.


DistCnC enabling
================

The RTM code is distCnC-ready with a tuner which implements a
good distribution plan.
Use the preprocessor definition _DIST_ to enable distCnC.
    
See the runtime_api reference for how to run distCnC programs.


Usage
=====

The command line is:

rtm_stencil nx ny nz ts
    nx  : positive integer for the number cells in x-direction
    ny  : positive integer for the number cells in y-direction
    nz  : positive integer for the number cells in z-direction
    ts  : positive integer for the number for time steps
