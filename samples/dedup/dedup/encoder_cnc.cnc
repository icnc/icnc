//
//                                                                                        CompressItems
//											 /	       \
//											/               \
// The pipeline for Encode is ProduceDataBlocks->ProduceDataItems->FilterItemsToCompress----------------->WriteItems
//

  // basic configuration data, read only once graph is running
[CONFIG configData: int tag];

  // The first stage of the encode pipeline is to break the input into blocks.
  // The blocks are enumerated, starting with zero.
  // This stage is done in the environment; a step is prescribed for each block.
[data_chunk dataBlocks : int blockCount] refcount_func=ConsumerBlockCount, dealloc_func=DeallocateBlock;
<dataBlockTags: int blockCount>;

env -> [dataBlocks], <dataBlockTags>;


  // The second stage of the pipeline breaks each chunk into a set of items, 
  // based on rabin fingerprints.
  // The items are enumerated by <block_id, item_position_in_block>
  // We also create a map of items per block; this is used to reassemble the items for output.
[data_chunk dataItems : int blockCount, int itemCount]  refcount_func=ConsumerItemCount, dealloc_func=DeallocateItem;
 <dataItemTags: int blockCount, int itemCount>;
[int itemsPerBlock: int blockCount]; // no refcount.  The count is reference for each item in a block.

<dataBlockTags>:: (ProduceDataItems);
[dataBlocks] -> (ProduceDataItems);
(ProduceDataItems) -> [dataItems], <dataItemTags>, [itemsPerBlock];


  // The third stage of the pipeline computes the SHA1 hash of the
  // item.  We create an item collection <hash, item> and a tag space
  // <hash>. The SHA1 hash identifies the block with (almost) perfect
  // certainty; we assume there are no collisions.  If we see the same
  // block twice, we will produce the same tag, and we rely on the CnC
  // system to ignore the update.
  //
  // We also pass along the item to write stage.
[data_chunk itemsToCompress : int hash]  refcount_func=ConsumerItemToCompressCount, dealloc_func=DeallocateItemToCompress;
 <hashTags: int hash>;
[data_chunk itemsToWrite : int blockCount, int itemCount]  refcount_func=ConsumerItemToWriteCount, dealloc_func=DeallocateItemToWrite;

<dataItemTags>:: (FilterItemsToCompress);
[dataItems] -> (FilterItemsToCompress);
(FilterItemsToCompress) -> [itemsToCompress], [itemsToWrite], <hashTags>;


  // The fourth stage of the pipeline compresses the items.
[data_chunk compressedItems : int hash]; // no refcount.  A compressedItem is potentially read many times (for each duplicate itemToWrite).

<hashTags>:: (CompressItems);
[itemsToCompress] -> (CompressItems);
(CompressItems) -> [compressedItems];


  // The final stage of the pipeline writes the items.  This is done in the environment.

[itemsPerBlock], [itemsToWrite],[compressedItems] -> env;

