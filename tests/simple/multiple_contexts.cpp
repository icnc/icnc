#include <iostream>

#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>

struct distmem_context;

struct nop_step
{
    template< typename CTXT >
    int execute( const int /*tag*/, CTXT & /*c*/ ) const
    {
        return CnC::CNC_Success;
    }
};

struct compute_on_all_distmem_tuner : public CnC::step_tuner<>
{
    int compute_on( const int t, distmem_context & /*c*/ ) const
    {
        return t % numProcs();
    }
};

struct distmem_context : public CnC::context< distmem_context >
{
    CnC::tag_collection< int > m_distmemTags;
    CnC::step_collection< nop_step, compute_on_all_distmem_tuner > m_distmemSteps;
    // The purpose of the item collection is so that the distmem_context will have a
    // different number of distributable items than the shmem_context (thus triggering
    // an error during execution).
    CnC::item_collection< int, int > m_distmemItems;
    distmem_context()
        : CnC::context< distmem_context >()
        , m_distmemTags( *this, "dt" )
        , m_distmemSteps( *this, "ds" )
        , m_distmemItems( *this )
    {
        m_distmemTags.prescribes( m_distmemSteps, *this );
        if( CnC::Internal::distributor::myPid() == 0 ) {
            CnC::debug::trace( m_distmemSteps, 3 );
            CnC::debug::trace( m_distmemTags );
        }
    }
};

struct distmem2_context;

struct compute_on_local_distmem2_tuner : public CnC::step_tuner<>
{
    int compute_on( const int t, distmem2_context & /*c*/ ) const
    {
        return ( t * 3 ) % numProcs();
    }
};

struct distmem2_context : public CnC::context< distmem2_context >
{
    CnC::tag_collection< int > m_distmem2Tags;
    CnC::step_collection< nop_step, compute_on_local_distmem2_tuner > m_distmem2Steps;
    distmem2_context()
        : CnC::context< distmem2_context >()
        , m_distmem2Tags( *this, "dt2" )
        , m_distmem2Steps( *this, "ds2" )
    {
        m_distmem2Tags.prescribes( m_distmem2Steps, *this );
        if( CnC::Internal::distributor::myPid() == 0 ) {
            CnC::debug::trace( m_distmem2Steps, 3 );
            CnC::debug::trace( m_distmem2Tags );
        }
    }
};

struct shmem_context : public CnC::context< shmem_context >
{
    CnC::tag_collection< int > m_shmemTags;
    CnC::step_collection< nop_step > m_shmemSteps;
    shmem_context()
        : CnC::context< shmem_context >()
        , m_shmemTags( *this, "sd" )
        , m_shmemSteps( *this, "ss" )
    {
        m_shmemTags.prescribes( m_shmemSteps, *this );
        if( CnC::Internal::distributor::myPid() == 0 ) {
            CnC::debug::trace( m_shmemSteps, 3 );
            CnC::debug::trace( m_shmemTags );
        }
    }
};

int main(int argc, char *argv[])
{
#ifdef _DIST_
    //    CnC::dist_cnc_init< distmem_context, shmem_context > dc_init;
    //CnC::dist_cnc_init< distmem_context > dc_init;
    CnC::dist_cnc_init< distmem2_context, distmem_context > dc_init;
#endif

    distmem_context distmemContext;
    for( int i = 0; i < 11; ++i ) {
        distmemContext.m_distmemTags.put( i );
    }

    distmem2_context distmem2Context;
    for( int i = 0; i < 11; ++i ) {
        distmem2Context.m_distmem2Tags.put( i );
    }

    shmem_context shmemContext;
    for( int i = 0; i < 11; ++i ) {
        shmemContext.m_shmemTags.put( i );
    }
    shmemContext.wait();
    distmemContext.wait();
    distmem2Context.wait();
    shmemContext.unsafe_reset();
    distmemContext.unsafe_reset();
    distmem2Context.unsafe_reset();

    return 0;
}
