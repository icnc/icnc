// parameter for relaxation
[double omega];

// number of rows in the tile
[int nRowsInTile];

// number of columns in the tile
[int nColumnsInTile];

// output to the current serial iteration
[Tile currTileMatrix] refcount_func=countAccess, dealloc_func=tileDestruct;

// prescribe the relaxation for the given tile
<tileSpace: int i,j,k>;


(relaxation) priority_func=diagonalPrior;
<tileSpace> :: (relaxation);
[currTileMatrix], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles], [omega] -> (relaxation) -> [currTileMatrix];

env -> <tileSpace>, [currTileMatrix], [omega], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles];
[currTileMatrix]-> env;

