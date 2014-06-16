//********************************************************************************
// Copyright (c) 2009-2014 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

#include "util.h"
#include "dedupdef.h"
#include "encoder.h"
#include "dedup_debug.h"
#include "hashtable.h"
#include "zlib.h"
#include "config.h"
#include "hashtable_private.h"
#include "rabin.h"
#include "queue.h"
#include "binheap.h"
#include "tree.h"
#include "encoder_util.h"
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
#endif

#include "cnc/cnc.h"
#include "cnc/debug.h"

typedef std::pair<int,int> pair;


int my_hash(int a, int b) 
{
    unsigned int h = a;
    unsigned int high = h & 0xf8000000;
    h = h << 5;
    h = h ^ (high >> 27);
    h = h ^ b;
            
    return int(h);
}

    

template <>
struct cnc_hash< pair >
{
    size_t operator()(const pair& p) const
    {
        return size_t(my_hash(p.first, p.second));
    }
};


class hash_key {
  public:
    int i0; int i1; int i2; int i3; int i4;

    hash_key( int a0, int a1, int a2, int a3, int a4 ) :
        i0(a0), i1(a1), i2(a2), i3(a3), i4(a4)
    {}
    hash_key( ) :
        i0(-1), i1(-1), i2(-1), i3(-1), i4(-1)
    {}
    bool operator==( const hash_key & o ) const
    { return i0 == o.i0 && i1 == o.i1 && i2 == o.i2 && i3 == o.i3 && i4 == o.i4; }
};

    

template <>
struct cnc_hash< hash_key >
{
    size_t operator()(const hash_key& p) const
    {
        int h1 = my_hash(p.i0, p.i1);
        int h2 = my_hash(h1, p.i2);
        int h3 = my_hash(h2, p.i3);
        int h4 = my_hash(h3, p.i4);
        return size_t(h4);
    }
};


struct my_context;

struct ProduceDataItems : public CnC::step_tuner<>
{
    int execute( const int & block_tag, my_context & c ) const;
    int priority( const int & tag, my_context & c ) const;
};

struct FilterItemsToCompress  : public CnC::step_tuner<>
{
    int execute( const pair & item_tag, my_context & c ) const;
    int priority( const pair & tag, my_context & c ) const;
};

struct CompressItems : public CnC::step_tuner<>
{
    int execute(const hash_key & hash_tag, my_context & c ) const;
    int priority( const hash_key & tag, my_context & c ) const;
};

template< typename TT, typename IT >
struct my_ituner : public CnC::hashmap_tuner
{
    int get_count( const TT & tag ) const
    {
        return 1;
    }
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< ProduceDataItems, ProduceDataItems >           m_produceSteps;
    CnC::step_collection< FilterItemsToCompress, FilterItemsToCompress > m_filterSteps;
    CnC::step_collection< CompressItems, CompressItems >                 m_compressSteps;

    CnC::tag_collection<int> m_dataBlockTags;
    CnC::tag_collection<pair> m_dataItemTags;
    CnC::tag_collection< hash_key, CnC::preserve_tuner< hash_key > > m_hashTags;

    CnC::item_collection<hash_key, data_chunk> m_compressedItems;
    CnC::item_collection<int, data_chunk, my_ituner< int, data_chunk > >            m_dataBlocks;
    CnC::item_collection<pair, data_chunk, my_ituner< pair, data_chunk > >          m_dataItems;
    CnC::item_collection<int, int>                                                  m_itemsPerBlock;
    CnC::item_collection<hash_key, data_chunk, my_ituner< hash_key, data_chunk > >  m_itemsToCompress;
    CnC::item_collection<pair, data_chunk, my_ituner< pair, data_chunk > >          m_itemsToWrite;
    my_context() 
        : CnC::context< my_context >(),
          m_produceSteps( *this ),
          m_filterSteps( *this ),
          m_compressSteps( *this ),

          m_dataBlockTags( *this ),
          m_dataItemTags( *this ),
          m_hashTags( *this ),  // preserve tags

          m_compressedItems( *this ), 
          m_dataBlocks( *this ),
          m_dataItems( *this ),
          m_itemsPerBlock( *this ),
          m_itemsToCompress( *this ),
          m_itemsToWrite( *this )
    {
        m_dataBlockTags.prescribes( m_produceSteps, *this );
        m_dataItemTags.prescribes( m_filterSteps, *this );
        m_hashTags.prescribes( m_compressSteps, *this );
#if 0
        CnC::debug::trace( m_dataBlockTags, "dbt" );
        CnC::debug::trace( m_dataItemTags, "dit" );
        CnC::debug::trace( m_hashTags, "ht" );
        CnC::debug::trace( m_compressedItems, "ci" );
        CnC::debug::trace( m_dataBlocks, "db" );
        CnC::debug::trace( m_dataItems, "di" );
        CnC::debug::trace( m_itemsPerBlock, "ipb" );
        CnC::debug::trace( m_itemsToCompress, "itc" );
        CnC::debug::trace( m_itemsToWrite, "itw" );
        CnC::debug::trace( ProduceDataItems(), "pdi") ;
        CnC::debug::trace( FilterItemsToCompress(), "fic" );
        CnC::debug::trace( CompressItems(), "ci" );
#endif        
        
    }
};

/*
 * The pipeline model for Encode is ProduceDataBlocks->ProduceDataItems->ChunkProcess->Compress->SendBlock
 */


/*--------------------------------------------------------------------------*/
/* 
 * read file, split it up, and send each block to a ProduceDataItems step
 *
 * This function is called from the environment, and it kicks off the 
 * pipeline,  creating blocks and prescribing a step to process 
 * each block
 */

int ProduceDataBlocks( my_context &c, config *config )
{
    u_char* src = (u_char*)malloc(MAXBUF);
    int blockCount = 0;
    int srclen = 0;
    while ((srclen = READ(config->input_fd, src, MAXBUF)) > 0) {
        ulong tmp = 0; 
        u_char* p = src;

        while (tmp < srclen) {
            u_char *anchor = src + tmp;          
            p = anchor + ANCHOR_JUMP;
            if (tmp + ANCHOR_JUMP >= srclen) {
                p = src + srclen;
            }
            data_chunk block;
            block.len = p-anchor;
            block.anchorid = blockCount;
            block.start = (u_char*)malloc(p - anchor + 1);
            memcpy(block.start, anchor, p-anchor);
            block.start[p-anchor] = 0;

            c.m_dataBlocks.put( blockCount, block );
            c.m_dataBlockTags.put( blockCount );
            tmp += ANCHOR_JUMP;
            blockCount++;
        }
    }
    free(src);
    return blockCount;
}


/*--------------------------------------------------------------------------*/
/* Split each block into smaller chunks we call items. 
 * Use rabin fingerprint to find the anchors used to split the blocks
 */
int ProduceDataItems::execute(const int & block_tag, my_context & c) const
{
    data_chunk block;
    c.m_dataBlocks.get( block_tag, block );
    // We delete the storage for block at the end of the step.

    u_int32 itemCount = 0;
    u_char * anchor = block.start;
    u_char* p = block.start;
    ulong n = MAX_RABIN_CHUNK_SIZE;

    while(p < block.start+block.len) {
        if (block.len + block.start - p < n) {
            n = block.len + block.start - p;
        }
        //find next anchor
        p = p + rabinseg(p, (int)n, 0);

        data_chunk item;
        item.start = (u_char *)malloc(p - anchor + 1);
        item.len = p-anchor;
        item.anchorid = block.anchorid;
        item.cid = itemCount;

        memcpy(item.start, anchor, p-anchor);
        item.start[p-anchor] = 0;

        c.m_dataItems.put(pair(item.anchorid, item.cid), item );
        c.m_dataItemTags.put(pair(item.anchorid, item.cid));

        itemCount++;
        anchor = p;
    } 
    c.m_itemsPerBlock.put(block.anchorid, itemCount);
    free(block.start);
    
    return CnC::CNC_Success;
}

int ProduceDataItems::priority( const int & tag, my_context & c ) const
{
    return tag;
};



/*--------------------------------------------------------------------------*/
/*
 * Filter items to be compressed.  Used the SHA1 hash as the tag. If we have seen it
 * before, the tag will match and the compression will not be performed.
 */
int FilterItemsToCompress::execute(const pair & item_tag, my_context &c) const
{
    data_chunk item;
    c.m_dataItems.get(item_tag, item);

    Calc_SHA1Sig(item.start, item.len, (uchar *)&item.sha1);

    int* iKey = (int *)item.sha1;
    c.m_hashTags.put(
        hash_key(iKey[0], iKey[1], iKey[2], iKey[3], iKey[4]));

    c.m_itemsToCompress.put( hash_key(iKey[0], iKey[1], iKey[2], iKey[3], iKey[4]),item );

    // create item to write; note we will actually write the compressed data.
    //
    //  we put the time in both itermsToCompress and itemsToWrite; we will delete
    // the storage when we write the item.
    //
    c.m_itemsToWrite.put( item_tag,item );

    return CnC::CNC_Success;
}


int FilterItemsToCompress::priority( const pair &tag, my_context &c ) const
{
    return tag.first;
};


/*--------------------------------------------------------------------------*/
/*
 * Compress the items
 */
int CompressItems::execute(const hash_key & hash_tag, my_context &c) const
{
    data_chunk item;
    c.m_itemsToCompress.get(hash_tag, item);

        /* compress the block */
    unsigned long len = (unsigned long)(item.len + (item.len >> 12) + (item.len >> 14) + 11);
    u_char *pstr = (byte *)malloc(len);

    if (compress(pstr, &len, item.start, (unsigned long)item.len) != Z_OK) {
        perror("compress");
        EXIT_TRACE1("compress() failed\n");
    }

    data_chunk compressedItem = item;
    compressedItem.len = len;
    compressedItem.start = pstr;

    int* iKey = (int *)item.sha1;
    c.m_compressedItems.put(
        hash_key(iKey[0], iKey[1], iKey[2], iKey[3], iKey[4]), compressedItem);

    return CnC::CNC_Success;
}

int CompressItems::priority( const hash_key &tag, my_context &c  ) const
{
    // top priority for compressing
    return 0;
};



static int
keys_equal_fn ( void *key1, void *key2 ){
  return (memcmp(((CacheKey*)key1)->sha1name, ((CacheKey*)key2)->sha1name, SHA1_LEN) == 0);
}


/*--------------------------------------------------------------------------*/
/*
 * This function is called from the environment.  It writes items to 
 * the file, one at a time, in order.   The first time we see an item,
 * we write the compressed data.  Subsequent times we write a fingerprint.
 */
void WriteItems( my_context& c, CONFIG config, int blockCount )
{

    HASHTABLE compressedItemsWritten = create_hashtable(65536, hash_from_key_fn, keys_equal_fn);
    if(compressedItemsWritten == NULL) {
      printf("ERROR: Out of memory\n");
      exit(1);
    }

    int itemsWrittenCount = 0;
    for (int blockId = 0; blockId < blockCount; blockId++ ) 
    {
        int itemsPerBlock;
        c.m_itemsPerBlock.get(blockId, itemsPerBlock);
        for (int itemId = 0; itemId < itemsPerBlock; itemId++) 
        {
            data_chunk item;
            c.m_itemsToWrite.get(pair(blockId,itemId), item);
    
            int* iKey = (int *)item.sha1;
            data_chunk compressedItem;
            c.m_compressedItems.get( hash_key(iKey[0], iKey[1], iKey[2], iKey[3], iKey[4]), compressedItem );

            // if we have seen it before, write a fingerprint
            if ((hashtable_search(compressedItemsWritten, (void *)item.sha1)) != NULL) {
                write_file(config->output_fd, TYPE_FINGERPRINT, itemsWrittenCount, SHA1_LEN, item.sha1);
            }
            else {
                // else write the compressed data and free the memory
                write_file(config->output_fd, TYPE_COMPRESS, 
                    itemsWrittenCount, compressedItem.len, compressedItem.start);
                free(compressedItem.start);
        
                // and write it to the hash table
                // we must allocate permanent storage for hashtable keys
                u_char *key= (byte *)malloc(SHA1_LEN);
                memcpy(key,item.sha1,SHA1_LEN);

                if (hashtable_insert(compressedItemsWritten, key, 0) == 0) {
                    EXIT_TRACE1("hashtable_insert failed");
                }
            }
            itemsWrittenCount++;
            free(item.start);
        }
    }
    hashtable_destroy(compressedItemsWritten,true);
}


/*--------------------------------------------------------------------------*/
/* Encode
 * Compress an input stream
 *
 * Arguments:
 *   conf:        Configuration parameters
 *
 */

void Encode(config* conf)
{
    //open input file and output files and write archive header
    conf->input_fd  = openInputFile(conf);
    conf->output_fd = writeHeader(conf);
    rabininit(0);

    my_context c;

    int blockcount = ProduceDataBlocks( c ,conf );
    WriteItems( c, conf, blockcount );
    c.wait();

    //finalize the archive
    writeFinish(conf->output_fd);
    CLOSE(conf->input_fd);
}
