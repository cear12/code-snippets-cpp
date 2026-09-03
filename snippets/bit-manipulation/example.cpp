// Bit Manipulation Cheat Sheet: small, widely-used bitwise tricks,
// exercised against real values so their effect is visible.
#include <bitset>
#include <iostream>

void printBits(const char* label, unsigned n) {
    std::cout << label << ": " << n << " = 0b" << std::bitset<8>(n) << "\n";
}

int main() {
    unsigned number = 0b0010'1010; // 42
    printBits("start", number);

    number |= (1 << 0); // set bit 0
    printBits("set bit 0", number);

    number &= ~(1 << 1); // clear bit 1
    printBits("clear bit 1", number);

    number ^= (1 << 3); // toggle bit 3
    printBits("toggle bit 3", number);

    for (unsigned n : {1u, 2u, 3u, 4u, 15u, 16u}) {
        bool isPowerOfTwo = (n & (n - 1)) == 0;
        std::cout << n << " is a power of two: " << std::boolalpha << isPowerOfTwo << "\n";
    }

    unsigned withLowBits = 0b0101'1100;
    printBits("before clearing lowest set bit", withLowBits);
    withLowBits = withLowBits & (withLowBits - 1);
    printBits("after clearing lowest set bit", withLowBits);

    int signedVal = 0b0101'1000;
    int lowestSetBit = signedVal & -signedVal;
    std::cout << "lowest set bit of 0b01011000 is 0b" << std::bitset<8>(lowestSetBit) << "\n";

    return 0;
}
