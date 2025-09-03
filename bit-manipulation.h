// set bit
number |= (1 << position);

// clear bit
number &= ~(1 << position);

// change bit
number ^= (1 << position);

// if n is a power of two
( n & ( n-1 ) ) == 0 

// resets the least significant non-zero bit of a number
n = ( n & ( n-1 ) )

// a quick way to "pick out" the least significant set bit.
n & -n