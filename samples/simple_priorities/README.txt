This is a simple test program to verify that step priorities are working.
The code was adapted from the fib_tuner example.

The program starts by prescribing 10 steps, numbered 0-9, in a shuffled order.
The steps are tuned with priorities corresponding to their tags.

Executing the ./run.sh script will build and run the program.
This script sets up the environment to use the FIFO_SINGLE scheduler,
and limits the CnC runtime to use a single thread for task execution.
Passing the argument "0" runs with priorities disabled, and 1 enables them.

We expect the steps to execute in the shuffled order when priorities aren't used,
but we'd expect them to be sorted when priorities are enabled.

The step priority is printed when the tuner is accessed (as a sanity check that the
priority is indeed being read), and the step tag is printed when the step executes.
