// Bit Manipulation Cheat Sheet: small, widely-used bitwise tricks,
// exercised against real values so their effect is visible.
#include <bitset>
#include <iostream>

void PrintBits(const char *label, unsigned n) {
  std::cout << label << ": " << n << " = 0b" << std::bitset<8>(n) << "\n";
}

int main() {
  unsigned number = 0b0010'1010; // 42
  PrintBits("start", number);

  number |= (1 << 0); // set bit 0
  PrintBits("set bit 0", number);

  number &= ~(1 << 1); // clear bit 1
  PrintBits("clear bit 1", number);

  number ^= (1 << 3); // toggle bit 3
  PrintBits("toggle bit 3", number);

  for (unsigned n : {1u, 2u, 3u, 4u, 15u, 16u}) {
    bool is_power_of_two = (n & (n - 1)) == 0;
    std::cout << n << " is a power of two: " << std::boolalpha
              << is_power_of_two << "\n";
  }

  unsigned with_low_bits = 0b0101'1100;
  PrintBits("before clearing lowest set bit", with_low_bits);
  with_low_bits = with_low_bits & (with_low_bits - 1);
  PrintBits("after clearing lowest set bit", with_low_bits);

  int signed_val = 0b0101'1000;
  int lowest_set_bit = signed_val & -signed_val;
  std::cout << "lowest set bit of 0b01011000 is 0b"
            << std::bitset<8>(lowest_set_bit) << "\n";

  return 0;
}
