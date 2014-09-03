// parameter for relaxation
[double omega];

// number of rows in the tile
[int nRowsInTile];

// number of columns in the tile
[int nColumnsInTile];

// number of rows of tiles
// commented out since the variable made global
// had to pass it into the priority function
// [int nRowsOfTiles];

// number of columns of tiles
// commented out because of the same reason above 
// [int nColumnsOfTiles];

// input to the current serial iteration
[double prevTileMatrix];

// output to the current serial iteration
[double currTileMatrix];

// prescribe the relaxation for the given tile
<tileSpace: int i,j>;

// prescribe the iteration
// decided to create the tileSpace at the serial part
// no need for another step to create them
//<iterationSpace: int i>;


(relaxation) priority_func=diagonalPrior;
<tileSpace> :: (relaxation);
[prevTileMatrix], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles], [omega] -> (relaxation) -> [currTileMatrix];

env -> <tileSpace>, [prevTileMatrix], [omega], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles];
[currTileMatrix]-> env;

