#ifndef WRAPPER_H
#define WRAPPER_H

// An interface graph for a library function. An expert needs to manually
// write a subclass of this class for each library function so that they
// can be composable with each other.
class wrapper : public CnC::graph
{
public:
    template <typename Ctx>
    wrapper(CnC::context<Ctx> &ctx) : CnC::graph(ctx, "wrapper") {}
    virtual void start() = 0;
    virtual void copyout() = 0;
    virtual void init(int total, ...) = 0;
};

#endif