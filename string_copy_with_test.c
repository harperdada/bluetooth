#include <stdio.h>
#include <string.h>
// Assuming your prototype looks like this:
// copy src to dest which is of size n
char* my_strcpy(char* dest, const char* src, int n)
{
    char *tmp = dest;
    int len = 0;

    if (!src || !dest || n < 0)  {
        printf("bad input\n");
        return NULL;
    }

    /* Determine the safe copy length: stop at the first null terminator
     * or when the destination capacity (n - 1) is reached. */
    while (src[len] != '\0' && len < n - 1)
        len++;

    dest[len] = '\0';

    //copy backwards from src to dest
    for (int i = len -1; i >= 0; i--) {
         dest[i] = src[i];
    }
   
    return tmp;
}

int main() {
    printf("--- Final Validation --- \n\n");

    // Test 1: Truncation logic
    char *src1 = "CouldWouldShould";
    char dest1[10];
    my_strcpy(dest1, src1, 10);
    printf("Test 1 (Truncation):\n");
    if (strlen(dest1) == 9 && dest1[9] == '\0') {
        printf("  Result: [PASS]\n\n");
    } else {
        printf("  Result: [FAIL]\n\n");
    }

    // Test 2: Exact fit
    char dest2[5];
    my_strcpy(dest2, "1234", 5);
    printf("Test 2 (Exact Fit):\n");
    if (strcmp(dest2, "1234") == 0) {
        printf("  Result: [PASS]\n\n");
    } else {
        printf("  Result: [FAIL]\n\n");
    }

    // Test 3: The Zero Size (UPDATED to match your new logic)
    printf("Test 3 (n = 0):\n");
    char buf3[10] = "KeepMe";
    char *res3 = my_strcpy(buf3, "ChangeMe", 0);
    // Now checking if it returns the original buffer, not NULL
    if (res3 == buf3 && strcmp(buf3, "KeepMe") == 0) {
        printf("  Result: [PASS] (Returned dest correctly)\n\n");
    } else {
        printf("  Result: [FAIL]\n\n");
    }

    // Test 4: Negative Size
    printf("Test 4 (n = -1):\n");
    char buf4[10] = "Alive";
    if (my_strcpy(buf4, "Dead", -1) == NULL && buf4[0] == 'A') {
        printf("  Result: [PASS]\n\n");
    } else {
        printf("  Result: [FAIL]\n\n");
    }

    // Test 5: NULL Trap
    printf("Test 5 (NULL Pointers):\n");
    if (my_strcpy(NULL, "src", 10) == NULL && my_strcpy(buf4, NULL, 10) == NULL) {
        printf("  Result: [PASS]\n\n");
    } else {
        printf("  Result: [FAIL]\n\n");
    }

    // Test 6: The Overlap Challenge (Dest > Src)
    printf("Test 6 (Overlapping Memory - Shift Right):\n");
    char overlap_buf[20] = "ABCDE";
    // We want to shift "ABCDE" one byte to the right to get "AABCDE"
    // src starts at 'A', dest starts at 'B'
    my_strcpy(overlap_buf + 1, overlap_buf, 6); 

    printf("  Expected: 'AABCD'\n"); // 'E' gets truncated by n=6 if we count \0
    printf("  Actual:   '%s'\n", overlap_buf);

    if (strcmp(overlap_buf, "AABCD") == 0) {
        printf("  Result: [PASS] (Overlap handled correctly!)\n\n");
    } else if (overlap_buf[2] == 'A') {
        printf("  Result: [FAIL] (Character bleed detected: '%s')\n\n", overlap_buf);
    } else {
        printf("  Result: [FAIL] (Unknown corruption)\n\n");
    }
    return 0;
}
