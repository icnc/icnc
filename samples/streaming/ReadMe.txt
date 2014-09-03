

This directory contains a few microbenchmarks that are focused on
stream processing.  These are not the most natural fit on CnC, but
they can be done.

For the standard threadring test (on vs-lin64-8)

  ghc                           -- 7.9 seconds

  ThreadRing                    -- 106.7 
  ThreadRing_OneStepCollection  -- > ~190

That basic ThreadRing test uses 16 threads, and keeps them busy via
work-stealing thrashing.  These also both use enormous amounts of
memory.  (Memory issues are now resolved.)  

With CNC_NUM_THREADS=1 we still use an enormous amountof memory (2gb), but
the time drops to:

  ThreadRing                    -- 35.65

Then, if we disable storing of tags it takes 2.88mb of RAM and clocks
in at:

  ThreadRing                    -- 15.49 seconds

Which is not bad.  It's near the top of the heap.

It would be helpful to write a raw TBB version to figure out how much
overhead we're adding.








