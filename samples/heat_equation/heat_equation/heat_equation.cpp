#if defined(_DIST_)
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <stdlib.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>

//#include <cnc/debug.h>

#include "tbb/tick_count.h"


#include <ctime>
#include <cstdio>

#define Eo(x) { std::cout << #x << " = " << (x) << std::endl; }

template<class T>
struct my_array
{
    int size;
    T* data;
    my_array(){}
    my_array(int s) {
        size = s;
        data = new T[size];
    }
    my_array(T* d, int s):data(d),size(s){ }
#ifdef _DIST_
    void serialize(CnC::serializer& ser) {
        ser & size;
        ser & CnC::chunk<T>(data, size);
    }
#endif
};

struct Pair {
    int It;
    int Jx;
    Pair() { }
    Pair(int i, int j) :It(i),Jx(j){ }
};

struct global_data{
    double k, hx, ht;
    int block_size, N_bl;
    double Xa,Xb,T0,T1;
    int Nx,Nt;
};

std::ostream& operator<<(std::ostream& os, const Pair& t){
    return os << t.It << ',' << t.Jx;
}


template <>
class cnc_tag_hash_compare< Pair > {
public:
    size_t hash(const Pair& tt) const {
        return size_t( (tt.It << 16) + tt.Jx );
    }

    bool equal(const Pair& a, const Pair& b) const {
        return ( ( a.It == b.It ) && ( a.Jx == b.Jx ) );
    }
};

struct HeatEquation_bl_context;
struct Step {
    int execute( const Pair & t, HeatEquation_bl_context & c ) const;
};

struct HeatEquation_bl_context : public CnC::context< HeatEquation_bl_context >
{
    CnC::step_collection< Step > Steps;
    CnC::item_collection< Pair, my_array<double> > H;
    CnC::tag_collection< Pair > Tag;
    CnC::item_collection<int,global_data> gd;

#pragma warning(push)
#pragma warning(disable: 4355)
    HeatEquation_bl_context()
        : CnC::context< HeatEquation_bl_context >(),
          Steps( *this ),
          H( *this ),
          Tag( *this ),
          gd( *this )
    {
        Tag.prescribes( Steps, *this );
        //        CnC::debug::collect_scheduler_statistics( *this );
    }
#pragma warning(pop)
};

//////////////////////////////////////////////////////////////////////////////////////
double Update( double x, double t, double k, double hx, double ht, double *oldU )
{
    return oldU[1] + ht * k * ( oldU[0] - 2.0*oldU[1] + oldU[2] ) / ( hx * hx );
}

double Analitical( double x, double t )
{
    return x + t;
}
////////////////////////////////////////////////////////////////////////////////////

#ifdef _DIST_
CNC_BITWISE_SERIALIZABLE( Pair );
CNC_POINTER_SERIALIZABLE( Pair );
CNC_BITWISE_SERIALIZABLE( global_data );
CNC_POINTER_SERIALIZABLE( global_data );
#endif

double calc_x(int k,double Xa, double Xb, int Nx){
    return Xa+((Xb-Xa)/double(Nx+1))*k;
}

int Step::execute( const Pair & tag, HeatEquation_bl_context & c ) const
{
    global_data d;
    c.gd.get(0,d);
    double k = d.k;
    double hx = d.hx;
    double ht = d.ht;
    int block_size = d.block_size;
    int N_bl = d.N_bl;
    double Xa = d.Xa;
    double Xb = d.Xb;
    double T0 = d.T0;
    double T1 = d.T1;
    int Nx = d.Nx;
    int Nt = d.Nt;
    int i,j;

    //    printf("%d %d %d\n",tag.It,tag.Jx,z);
    my_array<double> h[3];

    i = tag.It;
    j = tag.Jx;

    //    printf("@%03d:%03d",i,j);

    if(j) c.H.get( Pair( i-1, j-1 ),h[0]);
    c.H.get(Pair( i-1, j ),h[1]);
    if( j+1 < N_bl ) c.H.get(Pair( i-1, j+1 ),h[2]);

    double a[3];
    my_array<double> outdata(block_size);

    double t = T0+((T1-T0)/double(Nt+1))*i;

    int m_start = 0;
    int m_end = block_size;

    if (!j){
        outdata.data[0] = Analitical(Xa,t);
        m_start = 1;
    }
    if (j == (N_bl-1)) {
        m_end = block_size-1;
        outdata.data[block_size-1] = Analitical(Xb,t);
    }

    for(int m = m_start; m < m_end; m++) {
        if (block_size == 1){
            a[0] = h[0].data[block_size-1];
            a[1] = h[1].data[0];
            a[2] = h[2].data[0];
        } else {
            if(!m) {
                a[0] = h[0].data[block_size-1];
                a[1] = h[1].data[0];
                a[2] = h[1].data[1];
            } else if(m == block_size-1) {
                a[0] = h[1].data[block_size-2];
                a[1] = h[1].data[block_size-1];
                a[2] = h[2].data[0];
            } else {
                for(int n = 0; n < 3; n++){
                    a[n] = h[1].data[n+m-1];
                }
            }
        }

        double x = calc_x(m+block_size*j,Xa,Xb,Nx);
        outdata.data[m] = Update( x, t, k, hx, ht, &a[0] );
    }

    c.H.put(tag, outdata);
    if (i+1 <= Nt){
        Pair next_tag;
        next_tag.It = i+1;
        next_tag.Jx = j;
        c.Tag.put(next_tag);
    }

    //printf("!%03d:%03d",i,j), fflush(stdout);
    return CnC::CNC_Success;
}

int main (int argc, char **argv)
{
#if defined(_DIST_)
    CnC::dist_cnc_init< HeatEquation_bl_context > AAA;
#endif
    //    CnC::debug::set_num_threads(1);
    tbb::tick_count t0 = tbb::tick_count::now();
    if( argc < 3 ) {
        std::cerr << "expecting 2 arguments: <file> <blocksize>\n";
        exit( 1 );
    }

    global_data d;
    int& block_size = d.block_size;
    int& Nx = d.Nx;
    int& Nt = d.Nt;
    int& N_bl = d.N_bl;
    double& Xa = d.Xa;
    double& Xb = d.Xb;
    double& T0 = d.T0;
    double& T1 = d.T1;
    double& k = d.k;
    double& hx = d.hx;
    double& ht = d.ht;

    block_size = atoi(argv[2]);
    if(block_size < 1) {
        std::cerr<<"Bad block size\n";
        return 0;
    }

    {
        std::ifstream from(argv[1]);
        if( ! from ) {
            std::cerr << "couldn't open " << argv[1] << std::endl;
            exit( 2 );
        }
        from >> d.Xa >> d.Xb >> d.Nx >> d.T0 >> d.T1 >> d.Nt >> d.k;
        from.close();
    }


    if( block_size > Nx+1 ) {
        block_size = Nx+1;
    } else {
        Nx = ((Nx+1+block_size-1)/block_size)*block_size-1;
    }

    HeatEquation_bl_context c;

    hx = ( Xb - Xa ) / Nx;
    ht = ( T1 - T0 ) / Nt;
    //    for ( int i = 0; i < Nx + 1; i++ ) { c.X.put(i,Xa+i*hx); }
    //    for ( int i = 0; i < Nt + 1; i++ ) { c.T.put(i,T0+i*ht); }

    N_bl = (Nx+1) / block_size;

    c.gd.put(0,d);

    for ( int i = 0; i < N_bl; i++ ) {
        my_array<double> z(block_size);
        for(int j = 0; j < block_size; j++ ){
            double x = calc_x(block_size*i+j,Xa,Xb,Nx);
            z.data[j] = Analitical( x, T0 );
        }
        c.H.put(Pair(0,i),z);
    }

    Pair p;
    p.It = 1;
    for ( int j = 0; j < N_bl; j++ ) {
        p.Jx = j;
        c.Tag.put( p );
    }

    // Wait for all steps to finish
    c.wait();
    tbb::tick_count t1 = tbb::tick_count::now();
    std::cout<<"Time taken: "<< (t1-t0).seconds()<<" \n";

    if (argc >= 4){
        for (int i = 0; i <= Nt; i++)
            {
            for (int j = 0; j < N_bl; j++){
                my_array<double> z;
                c.H.get(Pair(i,j),z);
                for (int k = j*block_size; k < j*block_size+block_size && k <= Nx; k++){
                    printf("%.6lf ",double(z.data[k-j*block_size]));
                }
            }
            puts("");
        }
    }

    Eo(N_bl);
    Eo(Nt);
    return 0;
}
