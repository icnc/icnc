/* *******************************************************************************
 *  Copyright (c) 2007-2021, Intel Corporation
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/

/*
  CnC partitioner interfacem used by CnC tuners.
*/

#ifndef _DEFAULT_PARTITIONER_H_
#define _DEFAULT_PARTITIONER_H_

#include <cnc/internal/tbbcompat.h>
#include <tbb/info.h>
#include <cstdlib>

namespace CnC {

    class range_is_tag_type {};
    class range_is_range_type {};

    /// \brief Interface for partitioners: configuring how ranges are partitioned.
    ///
    /// The default_partitioner implements the template interface that each partitioner must satisfy.
    /// 
    /// Given a range type "R", a partitioner "P" must provide a copy-constructor
    /// and the following (template) interface:
    ///  - template< typename T > void divide_and_originate( R &, T & ) const;
    ///  - split_type
    ///
    /// The default_partitioner can be parametrized as follows:
    /// Set template argument to > 0 to let it use a fixed grainsize.
    /// If it equals 0, the grainsize is set to "original_rangeSize / #threads / 4"
    /// If it is < 0, the grainsize of the given range is obeyed.
    template< int grainSize = 0 >
    class default_partitioner
    {
    public:
        default_partitioner()
            : m_grainSize( grainSize ),
              m_nt( getenv( "CNC_NUM_THREADS" ) ?  atoi( getenv( "CNC_NUM_THREADS" ) ) : tbb::info::default_concurrency() )
        {
            //            std::cerr << "d";
        }
        default_partitioner( const default_partitioner< grainSize > & o )
            : m_grainSize( o.m_grainSize ),
              m_nt( getenv( "CNC_NUM_THREADS" ) ?  atoi( getenv( "CNC_NUM_THREADS" ) ) : tbb::info::default_concurrency() )
        {
            //            std::cerr << "c";
        }
        /// \brief divide given range into in arbitrary number of ranges of type Range
        ///
        /// Call si.originate_range( r ) for each new range.
        /// The original - but modified - range must *not* be passed to originate_range!
        /// If tag-types are self-dividing (e.g. if range-type == tag-type) you should call "originate"
        /// instead of "originate_range" for leaves of the recursive range-tree.
        /// \return true if the orignal - but modified - range needs further splitting, false if no further splitting is desired.
        ///
        /// The aggregated set of the members of the sub-ranges applied to "t.originate[_range]" must equal the set of member in given range.
        /// Overlapping ranges or gaps may lead to arbitrary effects.
        ///
        /// \param range the original range to split, may be altered
        /// \param si opaque object, call t.originate[_range]( r ) for all split-off sub-ranges
        template< typename Range, typename StepInstance >
        inline bool divide_and_originate( Range & range, StepInstance & si ) const;

        /// set to range_is_tag_type if tag is self-dividing, e.g. if the range-type is also the tag-type as passed to the step
        /// set to range_is_range_type if tag is undivisible, e.g. if range-type != step_type
        typedef range_is_range_type split_type;

    protected:
        /// \brief return true, if given range is divisible, false otherwise
        template< typename Range >
        bool is_divisible( const Range & range ) const;

        /// \return grainsize for given size of unsplitted range
        int grain_size( size_t fullRangeSize ) const;

        template< typename Range >
        bool is_divisible( const Range & range, int grain ) const;

    private:
        // actual implemenation of divide_and_originate, avoiding repeated access to range.size() and grain_size()
        template< typename Range, typename StepInstance >
        bool divide_and_originate( Range & range, StepInstance & si, const int grain ) const;
        int m_grainSize;
        int m_nt;
    };


    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    inline int default_partitioner< grainSize >::grain_size( size_t rangeSize ) const
    {
        if( grainSize != 0 ) return grainSize;
        else {
            if( m_grainSize <= 0 ) {
#define MX( _a, _b ) ((_a) > (_b) ? (_a) : (_b))
                const_cast< int & >( m_grainSize ) = rangeSize > 0 ? MX( 1, (int)(rangeSize / (size_t)m_nt / 4) ) : 1;
            }
            return m_grainSize;
        }
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    template< typename Range, typename StepInstance >
    inline bool default_partitioner< grainSize >::divide_and_originate( Range & range, StepInstance & si, const int grain ) const
    {
        if( this->is_divisible( range, grain ) ) {
            Range _r( range, tbb::split() );
            si.originate_range( _r );
        }
        return this->is_divisible( range, grain );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    template< typename Range, typename StepInstance >
    inline bool default_partitioner< grainSize >::divide_and_originate( Range & range, StepInstance & si ) const
    {
        return this->divide_and_originate( range, si, this->grain_size( range.size() ) );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    template< typename Range >
    inline bool default_partitioner< grainSize >::is_divisible( const Range & range ) const
    {
        return this->is_divisible( range, this->grain_size( range.size() ) );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    template< typename Range >
    bool default_partitioner< grainSize >::is_divisible( const Range & range, int grain ) const
    {
        return ( grainSize < 0 || (int)range.size() > grain ) && range.is_divisible();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// Use this instead of default_partitioner if your tag is self-dividing (e.g. a range)
    /// and you want to use the partitioning mechanism through cnC::tag_collection::put_range
    template< int grainSize = 0 >
    class tag_partitioner : public default_partitioner< grainSize >
    {
    public:
        template< typename Range, typename StepInstance >
        inline bool divide_and_originate( Range & range, StepInstance & si ) const;
        typedef range_is_tag_type split_type;

    private:
        template< typename Range, typename StepInstance >
        bool divide_and_originate( Range & range, StepInstance & si, const int grain ) const;
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    template< typename Range, typename StepInstance >
    inline bool tag_partitioner< grainSize >::divide_and_originate( Range & range, StepInstance & si, const int grain ) const
    {
        if( this->is_divisible( range, grain ) ) {
            Range _r( range, tbb::split() );
            si.originate_range( _r );
            if( this->is_divisible( range, grain ) ) return true;
        }
        si.originate( range );
        return false;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< int grainSize >
    template< typename Range, typename StepInstance >
    inline bool tag_partitioner< grainSize >::divide_and_originate( Range & range, StepInstance & si ) const
    {
        return this->divide_and_originate( range, si, this->grain_size( range.size() ) );
    }

} // namspace CnC

#endif // _DEFAULT_PARTITIONER_H_
