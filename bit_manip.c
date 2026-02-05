#include <stdio.h>
#include <stdint.h>

// Helper to visualize the bits
void print_binary(unsigned int n) {
    for (int i = 31; i >= 0; i--) {
        int k = n >> i;
        if (k & 1) printf("1");
        else printf("0");
        if (i % 8 == 0) printf(" "); // Space every byte for readability
    }
}

uint32_t get_bits(uint32_t num, int n, int p)
{
    uint32_t mask = ((1 << n) - 1) << p;
    uint32_t res = (num & mask) >> p;
    return res;
}

uint32_t set_bits(uint32_t num, int n, int p, uint32_t val)
{
    uint32_t mask = ((1 << n) - 1) << p;
    uint32_t res = (num & ~mask) | (val << p);
    return res;
}

void run_test(const char* test_name, uint32_t actual, uint32_t expected) {
    if (actual == expected) {
        printf("[ PASS ] %-40s | Result: 0x%08X\n", test_name, actual);
    } else {
        printf("[ FAIL ] %-40s | Expected: 0x%08X, Got: 0x%08X\n", 
               test_name, expected, actual);
    }
}

int main() {
    printf("--- Starting Bit Manipulation Validation ---\n\n");

    // --- get_bits Tests ---
    
    // 0xABCD1234, n=4, p=16 should extract the 'D' (0xD)
    run_test("get_bits: 4 bits at pos 16", 
              get_bits(0xABCD1234, 4, 16), 0xD);

    // 0xABCD1234, n=8, p=0 should extract 0x34
    run_test("get_bits: 8 bits at pos 0 (LSB)", 
              get_bits(0xABCD1234, 8, 0), 0x34);

    // 0xABCD1234, n=4, p=28 should extract 0xA
    run_test("get_bits: 4 bits at pos 28 (MSB)", 
              get_bits(0xABCD1234, 4, 28), 0xA);


    // --- set_bits Tests ---

    // 0xABCD1234, n=4, p=16, val=0x7 -> Expect 0xABC71234
    run_test("set_bits: Set 4 bits at pos 16 to 0x7", 
              set_bits(0xABCD1234, 4, 16, 0x7), 0xABC71234);

    // 0x0, n=4, p=0, val=0xF -> Expect 0x0000000F
    run_test("set_bits: Set LSB 4 bits to 0xF", 
              set_bits(0x0, 4, 0, 0xF), 0xF);

    // 0xFFFFFFFF, n=8, p=8, val=0x0 -> Expect 0xFFFF00FF
    run_test("set_bits: Clear 8 bits in middle", 
              set_bits(0xFFFFFFFF, 8, 8, 0x0), 0xFFFF00FF);

    // --- Safety/Edge Case Tests ---

    // Dirty Value: Passing 0xFF to a 4-bit slot should mask it to 0xF
    // 0x0, n=4, p=4, val=0xFF -> Expect 0x000000F0
    run_test("set_bits: Handling dirty value (val > n bits)", 
              set_bits(0x0, 4, 4, 0xFF), 0xF0);

    // n = 0: Should return original number (no-op)
    run_test("set_bits: n = 0 (No-op)", 
              set_bits(0x1234, 0, 4, 0xF), 0x1234);

    printf("\n--- Validation Complete ---\n");
    return 0;
}
