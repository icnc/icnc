#define _CRT_SECURE_NO_DEPRECATE

// define _DIST_ to enable distCnC
//#define _DIST_

#include <iostream>
#include <sstream>
#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif

class SimpleObj {
public:
    SimpleObj(int data = -1) : mData(data) {}
    int mData;
};

template< typename A, typename B >
class ComplexObj {
public:
    ComplexObj() : mOne(), mTwo() {}
    ComplexObj(A one, B two) : mOne(one), mTwo(two) {}
    A mOne;
    B mTwo;
};

typedef ComplexObj< int, int > ComplexObjType;

CNC_BITWISE_SERIALIZABLE( SimpleObj );
CNC_BITWISE_SERIALIZABLE( ComplexObjType );

CNC_POINTER_SERIALIZABLE( SimpleObj );
CNC_POINTER_SERIALIZABLE( ComplexObjType );

class my_context;

struct my_step
{
    int execute( const int & tag, my_context & c ) const;
};

struct my_context : public CnC::context< my_context >
{
    my_context()
        : CnC::context< my_context >(),
          m_steps( *this ),
          m_tags( *this ),
          m_simple( *this ),
          m_complex( *this )
          
    {
        m_tags.prescribes( m_steps, *this );
    }
    
    CnC::step_collection< my_step > m_steps;
    CnC::tag_collection< int > m_tags;
    CnC::item_collection< int, SimpleObj * > m_simple;
    CnC::item_collection< int, ComplexObjType * > m_complex;
};

int my_step::execute( const int & tag, my_context & c ) const
{
    SimpleObj * in;
    c.m_simple.get( tag, in );
    ComplexObjType * out = new ComplexObjType(in->mData, in->mData); 
    c.m_complex.put( tag, out );
    return CnC::CNC_Success;
}

int main( int, char *[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    my_context c;

    for( int i = 0; i<10; ++ i ) {
        c.m_tags.put( i );
        SimpleObj * t = new SimpleObj(i);
        c.m_simple.put( i, t );
    }
    c.wait();
    for( int i = 0; i<10; ++ i ) {
        ComplexObjType * out;
        c.m_complex.get( i, out );
        std::cout << out->mOne << " " 
                  << out->mTwo << std::endl;        
    }

    return 0;
}
