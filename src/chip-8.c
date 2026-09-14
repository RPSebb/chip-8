#include <string.h>
#include <stdint.h>

uint8_t ram[4096];
uint8_t registers[16];
uint8_t dt;
uint8_t st;
uint8_t keys[16];
uint8_t stack_pointer;
uint16_t I;
uint16_t program_counter;
uint16_t stack[16];
uint64_t pixels[32];

const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,
	0x20, 0x60, 0x20, 0x20, 0x70,
	0xF0, 0x10, 0xF0, 0x80, 0xF0,
	0xF0, 0x10, 0xF0, 0x10, 0xF0,
	0x90, 0x90, 0xF0, 0x10, 0x10,
	0xF0, 0x80, 0xF0, 0x10, 0xF0,
	0xF0, 0x80, 0xF0, 0x90, 0xF0,
	0xF0, 0x10, 0x20, 0x40, 0x40,
	0xF0, 0x90, 0xF0, 0x90, 0xF0,
	0xF0, 0x90, 0xF0, 0x10, 0xF0,
	0xF0, 0x90, 0xF0, 0x90, 0x90,
	0xE0, 0x90, 0xE0, 0x90, 0xE0,
	0xF0, 0x80, 0x80, 0x80, 0xF0,
	0xE0, 0x90, 0x90, 0x90, 0xE0,
	0xF0, 0x80, 0xF0, 0x80, 0xF0,
	0xF0, 0x80, 0xF0, 0x80, 0x80
};
static uint32_t rng_state = 12345; // seed de départ

uint8_t random(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return (uint8_t)(rng_state >> 16);
}

void process_event(uint8_t code, uint8_t state) {
    switch(code) {
        // Ligne 1 : & é " ' (1 2 3 4)  — Scancodes: 0x02, 0x03, 0x04, 0x05
        case 0x02: keys[0x1] = state; break;
        case 0x03: keys[0x2] = state; break;
        case 0x04: keys[0x3] = state; break;
        case 0x05: keys[0xC] = state; break;

        // Ligne 2 : A, Z, E, R — Scancodes: 0x1E, 0x11, 0x12, 0x13
        case 0x10: keys[0x4] = state; break;
        case 0x11: keys[0x5] = state; break;
        case 0x12: keys[0x6] = state; break;
        case 0x13: keys[0xD] = state; break;

        // Ligne 3 : Q, S, D, F — Scancodes: 0x10, 0x1F, 0x20, 0x21
        case 0x1E: keys[0x7] = state; break;
        case 0x1F: keys[0x8] = state; break;
        case 0x20: keys[0x9] = state; break;
        case 0x21: keys[0xE] = state; break;

        // Ligne 4 : W, X, C, V — Scancodes: 0x2C, 0x2D, 0x2E, 0x2F
        case 0x2C: keys[0xA] = state; break;
        case 0x2D: keys[0x0] = state; break;
        case 0x2E: keys[0xB] = state; break;
        case 0x2F: keys[0xF] = state; break;

        default: break;
    }
}

static inline void cls() { memset(pixels, 0, sizeof(pixels)); }

static inline void ret() {
    stack_pointer--;
    program_counter = stack[stack_pointer];
}

static inline void op_1(uint16_t opcode) {
    program_counter = (opcode & 0x0FFF);
}

static inline void op_2(uint16_t opcode) {
    stack[stack_pointer] = program_counter;
    stack_pointer++;
    program_counter = (opcode & 0x0FFF);
}

static inline void op_3(uint16_t opcode) {
    uint8_t x  = (opcode & 0x0F00) >> 8;
    uint8_t kk = opcode & 0x00FF;
    if(registers[x] == kk) { program_counter += 2; }
}

static inline void op_4(uint16_t opcode) {
    uint8_t x  = (opcode & 0x0F00) >> 8;
    uint8_t kk = opcode & 0x00FF;
    if(registers[x] != kk) { program_counter += 2; }
}

static inline void op_5(uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    if(registers[x] == registers[y]) { program_counter += 2; }
}

static inline void op_6(uint16_t opcode) {
    uint8_t x  = (opcode & 0x0F00) >> 8;
    uint8_t kk = opcode & 0x00FF;
    registers[x] = kk;
}

static inline void op_7(uint16_t opcode) {
    uint8_t x  = (opcode & 0x0F00) >> 8;
    uint8_t kk = opcode & 0x00FF;
    registers[x] += kk;
}

static inline void op_8(uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;

    switch(opcode & 0x000F) {
        case 0 : { registers[x]  = registers[y]; break; }
        case 1 : { registers[x] |= registers[y]; registers[15] = 0; break; }
        case 2 : { registers[x] &= registers[y]; registers[15] = 0; break; }
        case 3 : { registers[x] ^= registers[y]; registers[15] = 0; break; }
        case 4 : {
            uint16_t sum = registers[x] + registers[y];
            registers[15] = (uint8_t)(sum > 255);
            registers[x]  = (uint8_t)sum;
            break;
        }
        case 5 : {
            registers[15] = (uint8_t)(registers[x] >= registers[y]);
            registers[x] -= registers[y];
            break;
        }
        case  6 : {
            registers[15]  = registers[x] & 1; // copy last bit
            registers[x] = registers[y] >> 1;
            break;
        }
        case  7 : {
            registers[15] = (uint8_t)(registers[y] >= registers[x]);
            registers[x]  = registers[y] - registers[x];
            break;
        }
        case 14 : {
            registers[15]  = (uint8_t)(registers[x] > 127);
            registers[x] = registers[y] << 1;
            break;
        }
    }
}

static inline void op_9(uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    if(registers[x] != registers[y]) { program_counter += 2; }
}

static inline void op_A(uint16_t opcode) {
    uint16_t addr = (opcode & 0x0FFF);
    I = addr;
}

static inline void op_B(uint16_t opcode) {
    uint16_t addr = (opcode & 0x0FFF);
    program_counter = addr + registers[0];
}

static inline void op_C(uint16_t opcode) {
    uint8_t x  = (opcode & 0x0F00) >> 8;
    uint8_t kk = opcode & 0x00FF;
    uint8_t rnd = random();
    registers[x] = kk & rnd;
}

static inline void op_D(uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t n = (opcode & 0x000F);
    uint8_t pos_x = registers[x] % 64;
    uint8_t pos_y = registers[y] % 32;
    registers[15] = 0;

    for(uint8_t row = 0; row < n; row++) {
        uint8_t pixels_row = pos_y + row;
        if(pixels_row > 32) { return; }
        uint8_t byte = ram[I + row];
        uint64_t sprite = (uint64_t)byte << 56;
        // trunc pixel
        if(pos_x != 0) { sprite >>= pos_x; }

        if(pixels[pixels_row] & sprite) { registers[15] = 1; }
        pixels[pixels_row] ^= sprite;
    }
}

static inline void op_E(uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    switch(opcode & 0x00FF) {
        case 0x9E: {
            if(keys[registers[x]] == 1) { program_counter += 2; }
            break;
        }
        case 0xA1: {
            if(keys[registers[x]] == 0) { program_counter += 2; }
            break;
        }
        default: break;
    }
}

static inline void op_F(uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    switch(opcode & 0x00FF) {
        case 0x07: {
            registers[x] = dt;
            break;
        }
        // cycle on same instruction till keys is pressed
        case 0x0A: {
            int key_pressed = 0;
            for(int i = 0; i < 16; i++) {
                if(keys[i] != 0) {
                    registers[x] = i; // On sauvegarde le numéro de la touche (0 à 15)
                    key_pressed = 1;
                    break;            // Une touche a été trouvée, on sort de la boucle
                }
            }
            if(!key_pressed) { program_counter -= 2; }
            break;
        }
        case 0x15: {
            dt = registers[x];
            break;
        }
        case 0x18: {
            st = registers[x];
            break;
        }
        case 0x1E: {
            I += registers[x];
            break;
        }
        case 0x29: {
            I = 0x50 + registers[x] * 5;
            break;
        }
        case 0x33: {
            uint8_t value = registers[x];

            ram[I]     = value / 100;
            ram[I + 1] = (value / 10) % 10;
            ram[I + 2] = value % 10;
            break;
        }
        case 0x55: {
            for(uint8_t p = 0; p <= x; p++) {
                ram[I++] = registers[p];
            }
            break;
        }
        case 0x65: {
            for(uint8_t p = 0; p <= x; p++) {
                registers[p] = ram[I++] ;
            }
            break;
        }
        default: break;
    }
}

void process_instruction() {
    uint16_t opcode = (ram[program_counter] << 8) | ram[program_counter + 1];
    program_counter += 2;

    switch(opcode) {
        case 0x00E0: { cls(); break; }
        case 0x00EE: { ret(); break; }
        default: {
            switch(opcode & 0xF000) {
                case 0x1000: { op_1(opcode); break; }
                case 0x2000: { op_2(opcode); break; }
                case 0x3000: { op_3(opcode); break; }
                case 0x4000: { op_4(opcode); break; }
                case 0x5000: { op_5(opcode); break; }
                case 0x6000: { op_6(opcode); break; }
                case 0x7000: { op_7(opcode); break; }
                case 0x8000: { op_8(opcode); break; }
                case 0x9000: { op_9(opcode); break; }
                case 0xA000: { op_A(opcode); break; }
                case 0xB000: { op_B(opcode); break; }
                case 0xC000: { op_C(opcode); break; }
                case 0xD000: { op_D(opcode); break; }
                case 0xE000: { op_E(opcode); break; }
                case 0xF000: { op_F(opcode); break; }
                default: { break; }
            }
            break;
        }
    }
}
