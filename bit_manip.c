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


int main() {
    // Assumptions for these tests: 
    // get_bits(value, start_bit, length)
    // set_bits(value, start_bit, length, new_bits)
    
    unsigned int test_val = 0xABCD1234; // 10101011 11001101 00010010 00110100
    
    printf("--- Bit Manipulation Testing ---\n");
    printf("Original Value: 0x%X\n", test_val);
    printf("Binary:         "); print_binary(test_val); printf("\n\n");

    // 1. Test get_bits
    // Let's extract the 'D' (bits 16-19: 1101)
    unsigned int extracted = get_bits(test_val, 16, 4);
    printf("[Test get_bits]\n");
    printf("Extracting 4 bits starting at pos 16 (expecting 0xD / 1101)...\n");
    printf("Result: 0x%X (Binary: ", extracted);
    print_binary(extracted); printf(")\n\n");

    // 2. Test set_bits
    // Let's change that 'D' (1101) to a '7' (0111)
    unsigned int new_pattern = 0x7; 
    unsigned int modified = set_bits(test_val, 16, 4, new_pattern);
    
    printf("[Test set_bits]\n");
    printf("Setting 4 bits at pos 16 to 0x7...\n");
    printf("Original: "); print_binary(test_val); printf("\n");
    printf("Modified: "); print_binary(modified); printf("\n");
    printf("New Hex:  0x%X\n\n", modified);

    // 3. Edge Case: Setting bits at the very end (LSB)
    unsigned int lsb_test = set_bits(0x0, 0, 4, 0xF);
    printf("[Edge Case: LSB]\n");
    printf("Setting first 4 bits of 0x0 to 1111: 0x%X\n", lsb_test);

    return 0;
}
