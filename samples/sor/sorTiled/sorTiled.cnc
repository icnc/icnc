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
[double prevIterMatrix];

// output to the current serial iteration
[double currIterMatrix];

// prescribe the relaxation for the given tile
<tileSpace: int i,j>;

// prescribe the iteration
//<iterationSpace: int i>;

<iterationSpace> :: (iterationStep);
[nRowsOfTiles], [nColumnsOfTiles] -> (iterationStep) -> <tileSpace>;

<tileSpace> :: (relaxation);
[prevIterMatrix], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles], [omega] -> (relaxation) -> [currIterMatrix];

env -><iterationSpace>, [prevIterMatrix], [omega], [nRowsInTile], [nColumnsInTile], [nRowsOfTiles], [nColumnsOfTiles];
[currIterMatrix]-> env;

