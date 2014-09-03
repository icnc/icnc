// parameter for relaxation
[double omega];

// number of rows in the tile
[int nRowsInTile];

// number of columns in the tile
[int nColumnsInTile];

// number of rows of tiles
[int nRowsOfTiles];

// number of columns of tiles
[int nColumnsOfTiles];

// input to the current serial iteration
[double prevTileMatrix];

// output to the current serial iteration
[double currTileMatrix];

// prescribe the relaxation for the given tile
<tileSpace: int i,j>;

// prescribe the iteration
//<iterationSpace: int i>;

<iterationSpace> :: (iterationStep);
[nRowsOfTiles], [nColumnsOfTiles] -> (iterationStep) -> <tileSpace>;

<tileSpace> :: (relaxation);
[prevTileMatrix], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles], [omega] -> (relaxation) -> [currTileMatrix];

env -><iterationSpace>, [prevTileMatrix], [omega], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles];
[currTileMatrix]-> env;

