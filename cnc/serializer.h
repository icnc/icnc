/* *******************************************************************************
 *  Copyright (c) 2007-2014, Intel Corporation
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
  Marshalling interface for distributed CnC.
 */

#ifndef _CNC_SERIALIZER_H_
#define _CNC_SERIALIZER_H_

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/scalable_object.h>
#include <complex>
#include <vector>

#include <cnc/internal/cnc_stddef.h> // size_type

#include <memory> // std::allocator

namespace CnC
{
    class CNC_API serializer;
    class no_alloc{};

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
   \defgroup serialization Serialization
    @{
   
    There are three possible ways of setting up the serialization
    of your struct resp. class:
   
    \defgroup ser_bit Serialization of simple structs/classes without pointers or virtual functions.
     @{
       If your struct does not contain pointers (nor virtual
       functions), instances of it may be serialized by simply
       copying them bytewise. In this case, it suffices to
       add your struct to the category bitwise_serializable which is done
       by providing the following global function:
       \code
           inline bitwise_serializable serializer_category( const Your_Class * ) {
               return bitwise_serializable();
           }
       \endcode
       As a short-cut, you can just use #CNC_BITWISE_SERIALIZABLE( T ) to declare your class as bitwise serializable:
       \code
       CNC_BITWISE_SERIALIZABLE( my_class );
       \endcode
    @}
    \defgroup ser_exp Non bitwise serializable objects
     @{
       You'll have to provide a serializer function for your class.
       One way of doing this is to derive your class from
       serializable and to provide a method "serialize":
       \code
           class Your_class : public serializable {
               ...
           public:
               void serialize( CnC::serializer & );
               ...
           };
       \endcode
       In the implementation of this serialize method you should
       use the operator& of the serializer class. Here is an example:
       \code
           class Your_class : public serializable {
           private:
               double x;
               float  y;
               int    len;
               char*  arr; // array of lenth len
               ...
           public:
               void serialize( CnC::serializer & buf ) {
                   buf & x
                       & y
                       & len;
                   buf & CnC::chunk< char >( arr, len_arr ); // chunk declared below
                   ...
               }
           }
       \endcode
       This method will called for both packing and unpacking
       (what will actually be done, depends on the internal mode
       of the serializer.)
       \n
       Alternatively, you can provide a global function \n
       \code
           void serialize( serializer& buf, Your_class& obj );
       \endcode
       which should use the operator& of the serializer class
       (same as for method 2. above). This is useful in cases
       where you want to leave your class unchanged.
    @}
    \defgroup ser_ptr Marshalling pointer types (e.g. items which are pointers)
     @{
       A common optimization used by CnC programs targeting the
       shared-memory runtime is to store pointers to large objects
       inside of item collections instead of copies of the objects.
       This is done to minimize the overhead of copying into and out
       of the item collection.  Programs written in this style can be
       sued with distCnC, but special care needs to be taken.
       \n
       Distributed CnC requires that data stored in item collections be
       serializable in order to communicate data between different
       processes.  Consequently, the programmer must modify such
       pointer-using CnC programs in two ways in order to use the
       distributed-memory runtime.  First, the programmer must provide a
       specialization of CnC::serialize for the pointer type pointing to T:
       \code
       void CnC::serialize( CnC::serializer & ser, T *& ptr ) {
           ser & CnC::chunk< Your_class[, allocator] >( ptr, 1 );
       }
       \endcode
        Please refer to CnC::chunk to learn about appropriate memory
        management.
       \n
       If the pointer represents a single object and not a C-style array,
       then the programmer may the use convenience macro:
       \code CNC_POINTER_SERIALIZABLE( T ); \endcode
       This macro implements the above method for the case when the
       pointer refers to only a single object (length == 1).  Note that
       if T is a type that contains template parameters, the programmer
       should create a typedef of the specific type being serialized and
       use this type as the argument for the macro.
       \n
       Next, programmers must ensure that type T itself implements a
       default constructor and is serializable by itself. The latter 
       as achieved as described above.
       It is the same method that programmers must provide if storing
       copies of objects instead of pointers to objects.
       \n
       Reading from and writing to item collections is not different in
       this style of writing CnC programs.  However, there are a few
       things to note:
       \n
       When an object is communicated from one process to another, an
       item must be allocated on the receiving end. By default, CnC::chunk
       uses the default allocator for this and so requires the default constructor 
       of T. After object creation, any fields are updated by the provided
       serialize methods. Thios default behavior simplifies the object
       allocation/deallocation: most of it is done automatically behind the scenes.
       The programmer should just pass an unitialized pointer to
       get().  The pointer will then point to the allocated object/array after
       the get completes.
       \n
       In advanced and potentially dangerous setups, the programmer might wish
       to store the object into memory that has been previously allocated.
       This can be done by an explicit copy and explicit object lifetime control.
       Please note that such a scenario can easily lead to uses of CnC which 
       do not comply with CnC's methodology, e.g. no side effects in steps and
       dynamic single assigments.
   
       \defgroup seralloc Serialization of std::shared_ptr
       @{
         std::shared_ptr are normally supported out-of-the-box (given the underlying type
         is serializable (similar to \ref ser_ptr).
   
         Only shared_ptrs to more complex pointer-types require special treatment
         to guarantee correct garbage collection. If
         - using std::shared:ptr
         - AND the pointer-class holds data which is dynamically allocated
         - AND this dynamic memory is NOT allocated in the default constructor but during marshalling with CnC::chunk
   
         then (and only then) you might want to use construct_array and destruct_array.
         It is probably most convenient to 
         - use the standard CnC::chunk mechanism during serialization
         - allocate dynamic memory in the constructors with construct_array
         - and deallocate it in the destructor using destruct_array
   
         This mechanism is used in the cholesky example which comes with this distribution.
   
         Alternatively you can use matching
         allocation/deallocation explicitly in your con/destructors and during serialization.
       @}
     @}
**/

    class serializable {
    public:
        // must provide member function
        // void serialize( CnC::serializer& buf ) {
        //     /* use serializer::operator& for accessing buf */
        // }
    };

    template< class T > inline
    void serialize( serializer & buf, T & obj ) {
        obj.serialize( buf );
    }


    /// Specifies serialization category: explicit serialization via a "serialize" function
    class explicitly_serializable {};

    /// General case: a type belongs to the category explicitly_serializable,
    /// unless there is a specialization of the function template
    /// "serializer_category" (see below).
    template< class T > inline
    explicitly_serializable serializer_category( const T * )
    {
        return explicitly_serializable();
    }

    /// simple structs/classes are bitwise serializable.

    /// Specifies serialization category: byte-wise copy
    class bitwise_serializable {};

    /// \def CNC_BITWISE_SERIALIZABLE( T )
    /// Convenience macro for defining that a type should be treated
    /// as bitwise serializable:
#   define CNC_BITWISE_SERIALIZABLE( T ) \
        static inline CnC::bitwise_serializable serializer_category( const T * ) { \
            return CnC::bitwise_serializable(); \
        }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class A, class B > inline
    void serialize( CnC::serializer & buf, std::pair< A, B > & obj ) {
        buf & obj.first & obj.second;
    }

    template< class T >
    inline CnC::bitwise_serializable serializer_category( const std::complex< T > * ) {
        return CnC::bitwise_serializable();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /**
       \class chunk
       \brief Serialization of arrays with and without automatic memory handling.
       
       Specified (to serializer::operator&) via
       \code
           CnC::chunk< type[, allocator] >( your_ptr, len )
       \endcode
       where your_ptr is a pointer variable to your array of <type>s.
       
       If the allocator is the special type CnC::no_alloc, the runtime assumes that 
       the programer appropriately allocates (if ser.is_unpacking())
       and deallocates (if ser:is_cleaning_up()) the array. This implies that
       the using serialize method/function needs to make sure it passes valid and correct
       pointers to chunk.
        
       If not CnC::no_alloc , the Allocator class must meet "allocator" requirements of ISO C++ Standard, Section 20.1.5
        
       When unpacking, your_ptr must be NULL, and it is allocated automatically using
       the given allocator and when cleaning up, your_ptr is deallocated accordingly.
       
       If using an allocator (in particular hre default std::allocator) object/array 
       allocation and deallocation is greatly simplified for the programmer.
       There is no need to allocate and/or deallocate objects/arrays manually.
    **/
    template< class T, class Allocator = std::allocator< T > >
    struct chunk
    {
        chunk( T *& arr, size_type len ) : m_arrVar( arr ), m_len( len ) {}
        template< template< class I > class Range, class J >
        chunk( T *& arr, const Range< J > & range )
            : m_arrVar( arr ), m_len( range.size() )
        {
            CNC_ASSERT( range.begin() == 0 );
        }

        T *& m_arrVar;
        size_type m_len;
    };

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// Convenience macro for declaring serializable pointers.
    /// The object being pointed to should be serializable.

#    define CNC_POINTER_SERIALIZABLE( T )                       \
    namespace CnC {                                             \
        static inline void serialize( serializer & ser, T *& t ) {  \
            ser & chunk< T >(t, 1);                        \
        }                                                       \
    }
    
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// \defgroup serspec Automatic serialization of built-in types.
    /// They can be copied byte-wise.
    ///
    /// In order to add your own class to the category bitwise_serializable,
    /// add a specialization of "serializer_category" returning bitwise_serializable for your class.
    /// (e.g. by using CnC::CNC_BITWISE_SERIALIZABLE).
    ///  @{
    CNC_BITWISE_SERIALIZABLE( bool );               ///< bool is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( char );               ///< char is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( signed char );        ///< signed char is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( unsigned char );      ///< unsigend char is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( short );              ///< short is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( unsigned short );     ///< unsigend short is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( int );                ///< int is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( unsigned int );       ///< unsigned int is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( long );               ///< long is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( unsigned long );      ///< unsigned long is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( long long );          ///< long long is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( unsigned long long ); ///< unsigned long long is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( float );              ///< float is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( double );             ///< double is bitwise serializable
    CNC_BITWISE_SERIALIZABLE( long double );        ///< long double is bitwise serializable
    ///  @}

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    namespace Internal {
        class Buffer;
        class BufferAccess;
    }

    /// \brief Handles serilialization of data-objects.
    ///
    /// objects of this class are passed to serialization methods/functions.
    /// Try to use operator & only; using anything else is potentially dangerous.
    class CNC_API serializer : public Internal::scalable_object
    {
    public:
        enum Mode {
            MODE_PACKED_SIZE,
            MODE_PACK,
            MODE_UNPACK,
            MODE_CLEANUP
        };

        inline serializer( bool addCRC = false, bool addSize = false );
        inline ~serializer();

        /// (Re)allocates a buffer of the given size.
        inline void resize( size_type len );

        /// Swap internal representations of the given two serializers.
        friend void swap( serializer & s1, serializer & s2 );

        inline void set_mode_packed_size();
        inline void set_mode_pack();
        inline void set_mode_unpack();
        inline void set_mode_cleanup();
        inline void set_mode_pack( bool addCRC, bool addSize );
        inline void set_mode_unpack( bool addCRC, bool addSize );
        bool is_packing() const { return m_mode == MODE_PACK; }
        bool is_unpacking() const { return m_mode == MODE_UNPACK; }
        bool is_cleaning_up() const { return m_mode == MODE_CLEANUP; }
        size_type get_size() const { return m_packedSize; }
        /// @return size of header
        inline size_type get_header_size() const;
        /// @return the number of bytes already packed into the buffer
        inline size_type get_body_size() const;
        /// @return total number of bytes in buffer (header + data)
        inline size_type get_total_size() const;
        /// @return pointer to message header
        inline void * get_header() const;
        /// @return pointer to the message body (without header)
        inline void * get_body() const;
        /// @return body size from header, -1 if something goes wrong
        inline size_type unpack_header() const;

        /// Top-level packing interface, to be used in the "serialize"
        /// function of your classes.
        /// Applies the serializer to one object resp. an array of objects
        /// (e.g. packing them into the serializer or unpacking them from
        /// the serializer).
        /// Dispatches w.r.t. the packing category of the object's type
        /// (i.e. byte-wise copying or object serialization).
        template< class T > serializer & operator&( T & var );
        template< class T > serializer & operator&( const T & var );
        //template< class T > serializer & operator&( Internal::array_no_alloc_type< T > a ); //< used via array_no_alloc( yourArr, len )
        template< class T, class Allocator >
            serializer & operator&( chunk< T, Allocator > a ); //< used via auto_array( yourArrVar, len )

        class reserved;
        /// reserve current position in buffer to be filled later with a call to "complete"
        /// rserve/complete supported for bitwise-serializable types only
        template< class T > reserved reserve( const T & _obj );
        
        /// fill in data at position that has been reserved before
        /// rserve/complete supported for bitwise-serializable types only
        template< class T > void complete( const reserved & r, const T & _obj );

        // Low-level packing interface:
        template< class T > inline size_type packed_size( const T * arr, size_type len ) const {
            return packed_size( arr, len, serializer_category( arr ) );
        }
        template< class T > inline void pack( const T * arr, size_type len ) {
            pack( arr, len, serializer_category( arr ) );
        }
        template< class T > inline void unpack( T * arr, size_type len ) {
            unpack( arr, len, serializer_category( arr ) );
        }
        template< class T > inline void cleanup( T * arr, size_type len ) {
            cleanup( arr, len, serializer_category( arr ) );
        }
        template< class T > inline size_type packed_size( const T & var ) const { return packed_size( &var, 1 ); }
        template< class T > inline void pack( const T & var ) { return pack( &var, 1 ); }
        template< class T > inline void unpack( T & var ) { return unpack( &var, 1 ); }
        template< class T > inline void cleanup( T & var ) { return cleanup( &var, 1 ); }

        inline serializer( const serializer& );

        /// Allocates an array of type T and size num in pointer variable arrVar
        template< class T, class Allocator = std::allocator< T > >
        struct construct_array
        {
            construct_array( T *& arrVar, size_type num );
        };
        /// destructs the array of type T and isze num at arrVar and resets arrVar to NULL.
        template< class T, class Allocator = std::allocator< T > >
        struct destruct_array
        {
            destruct_array( T *& arrVar, size_type num );
        };
        ///  @}
        template< class T >
        struct construct_array< T, no_alloc >
        {
            construct_array( T *&, size_type ){}
        };
        template< class T >
        struct destruct_array< T, no_alloc >
        {
            destruct_array( T *&, size_type ){}
        };

    private:
        template< class T > inline size_type packed_size( const T * arr, size_type len, bitwise_serializable ) const;
        template< class T > inline void pack( const T * arr, size_type len, bitwise_serializable );
        template< class T > inline void unpack( T * arr, size_type len, bitwise_serializable );
        template< class T > inline void cleanup( T * arr, size_type len, bitwise_serializable );
        template< class T > inline size_type packed_size( const T * arr, size_type len, explicitly_serializable ) const;
        template< class T > inline void pack( const T * arr, size_type len, explicitly_serializable );
        template< class T > inline void unpack( T * arr, size_type len, explicitly_serializable );
        template< class T > inline void cleanup( T * arr, size_type len, explicitly_serializable );


        /// Don't allow copying
        void operator=( const serializer& );

        /// misc:
        size_type remaining_capacity() const;

        Internal::Buffer * m_buf;
        size_type          m_packedSize; /// < for MODE_PACKED_SIZE only
        Mode               m_mode;
        
        friend class Internal::BufferAccess;
    };

    ///  @}

} // namespace CnC

#include <cnc/internal/dist/Serializer.impl.h>

#endif // _CNC_SERIALIZER_H_
