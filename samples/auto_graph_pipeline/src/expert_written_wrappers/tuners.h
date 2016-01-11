#ifndef TUNERS_H
#define TUNERS_H

#include <vector>
#include <iostream>

typedef enum {
    _ROW_BLOCK_CYCLIC_ = 0
} distribution_type;

// (starting row, starting column) pair to index a block
struct RC {
    int   row;
    int   col;
};
CNC_BITWISE_SERIALIZABLE(RC);

std::ostream& cnc_format( std::ostream& output, const RC& c)
{
    return (output << c.row << "," << c.col);
}

template <>
class cnc_tag_hash_compare<RC>
{
public:
    size_t hash(const RC & t) const {
        return (t.row << 10) + t.col;      
    }
    
    bool equal(const RC &a, const RC & b) const {
        return (a.row == b.row) && (a.col == b.col); 
    }
};

// our own definition of a tile/block
// stored in data collections
template< typename T >
class Block
{
public:
    Block() : num_rows(0), num_columns(0) {}
    Block( int rows, int columns) : num_rows(rows), num_columns(columns) {
        block.resize(rows * columns); 
    }
    
    T *data() { 
        return block.data(); 
    }

#ifdef _DIST_
    void serialize( CnC::serializer & s ) {
        s & num_rows & num_columns & block;
    }
#endif

    std::ostream& cnc_format( std::ostream& output) const
    {
        return (output << num_rows << "," << num_columns << "," << &block);
    }

private:
    int num_rows;
    int num_columns;
    std::vector<T> block; // a linear buffer
};

std::ostream& cnc_format( std::ostream& output, const std::shared_ptr< Block< double > > & c)
{
    return c->cnc_format(output);
}

inline int row_process(int row, int rows_per_block)
{
    return (row / rows_per_block) % CnC::tuner_base::numProcs();
}
    
// This tuner is used as both an item tuner, and a step tuner. So it has
// not only consumed on, but also compute_on
class row_wise_distribution_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
public: 
    row_wise_distribution_tuner() : rows_per_block(0) {}
    row_wise_distribution_tuner(int n) : rows_per_block(n) {}
    
    int consumed_on(const RC &tag) const {
        return row_process(tag.row, rows_per_block);
    }
    
    template< typename Arg >
    int compute_on(const RC &tag, const Arg &) const {
        return row_process(tag.row, rows_per_block);
    }
private:
    int rows_per_block;
};
CNC_BITWISE_SERIALIZABLE(row_wise_distribution_tuner);

class col_wise_distribution_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
public: 
    col_wise_distribution_tuner() : cols_per_block(0) {}
    col_wise_distribution_tuner(int n) : cols_per_block(n) {}
    
    int consumed_on(const RC &tag) const {
        return CnC::CONSUMER_ALL;
    }

private:
    int cols_per_block;
};
CNC_BITWISE_SERIALIZABLE(col_wise_distribution_tuner);

// Note: given C=A*B, BLOCK_ROWS is for array A and C: they are row blocked
// (several adjacent entire rows as a block). Array B is not blocked. This is
// a very simplistic choice only for convenience.
#define BLOCK_ROWS BR

#endif  


