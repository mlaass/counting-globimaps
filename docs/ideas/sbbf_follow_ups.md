
* quadtree organization for querying
* content aware sfc, adaptive hilbert curve?
* more memory efficient hilbert 2d+3d lut with partial gen using BMI2 PDEP/PEXT or bit-magic?
* can we learn the transformations?
* other adaptive sfcs that minimize block index overlap ?
    - e.g. hilbert + interleaved offsets (index, offset) tuples mapping from sfc(v)-> sfc_interleaved(v)? how can we generate the optimal offsets + indices?
    - try starting random
    - optimization algorithm that optimizes the offset + index tuples to achieve even distribution of positive points across block_indices:
        1. pass 1: calculate block index histogram + min + max windows on sfc(v)
        2. pass 2: move max window to closest min window with a positive or negative shift
        3. keep iterating along sfc(v) and repeat 1. + 2.
    -