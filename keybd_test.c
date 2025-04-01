#define PS2_BASE 0xFF200100

// Define the global variable declared in the header
volatile char ps2_keyboard_code = 0;

void initPS2Keyboard(void) {
    volatile int *PS2_ptr = (int *)PS2_BASE;
    *PS2_ptr = 0xFF;  // Send reset command
}

char readPS2ScanCode(void) {
    volatile int *PS2_ptr = (int *)PS2_BASE;
    int PS2_data = *PS2_ptr;
    int RVALID = PS2_data & 0x8000;
    char scanCode = 0;
    
    if (RVALID) {
        // Valid data is available: extract the scan code
        scanCode = PS2_data & 0xFF;
        ps2_keyboard_code = scanCode;
        
        // If self-test passed (0xAA), send the enable scanning command (0xF4)
        if (scanCode == 0xAA) {
            *PS2_ptr = 0xF4;
        }
        return scanCode;
    }
    
    // No valid data available; return 0 (or another code you reserve for “no key”)
    return 0;
}

int get_block_index_from_scan_code(char code) {
    switch (code) {
        case 0x16: return 0; // Digit 1
        case 0x1E: return 1; // Digit 2
        case 0x26: return 2; // Digit 3
        case 0x25: return 3; // Digit 4
        case 0x2E: return 4; // Digit 5
        case 0x36: return 5; // Digit 6
        case 0x3D: return 6; // Digit 7
        case 0x3E: return 7; // Digit 8
        case 0x46: return 8; // Digit 9
        case 0x45: return 9; // Digit 0
        case 0x4D: return 10; // '-' (optional)
        default:   return -1;
    }
}
