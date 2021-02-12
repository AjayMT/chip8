
#include <SDL2/SDL.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define SCREEN_SIZE (64 * 32)

char *filename = NULL;

uint16_t opcode;
uint8_t memory[0x1000];
uint8_t V[0x10];
uint16_t I;
uint16_t pc;
uint16_t stack[0x10];
uint16_t sp;

uint8_t screen[SCREEN_SIZE];
uint8_t keys[0x10];
uint8_t delay_timer;
uint8_t sound_timer;

uint8_t draw_flag = 0;

uint8_t chip8_fontset[] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint8_t keymap[0x10] = {
};

uint8_t init()
{
  pc = 0x200;
  opcode = 0;
  I = 0;
  sp = 0;
  memset(V, 0, sizeof(V));
  memset(stack, 0, sizeof(stack));
  memset(memory, 0, sizeof(memory));
  memset(screen, 0, sizeof(screen));
  memset(keys, 0, sizeof(keys));
  delay_timer = 0;
  sound_timer = 0;

  // Load fontset
  memcpy(memory, chip8_fontset, 80);

  // Load program
  FILE *f = fopen(filename, "r");
  if (f == NULL) return 1;
  if (fseek(f, 0, SEEK_END)) return 1;
  uint32_t len = ftell(f);
  if (fseek(f, 0, SEEK_SET)) return 1;
  uint8_t *buffer = malloc(len);
  fread(buffer, 1, len, f);
  memcpy(memory + 512, buffer, len < (4096 - 512) ? len : (4096 - 512));
  free(buffer);
  fclose(f);

  return 0;
}

uint8_t cycle()
{
  // emulate one cycle
  opcode = (memory[pc] << 8) | memory[pc + 1];
  uint8_t x;
  switch (opcode & 0xF000) {
  case 0:
    switch (opcode & 0xF) {
    case 0: // clear screen
      memset(screen, 0, sizeof(screen));
      draw_flag = 1;
      pc += 2;
      return 0;

    case 0xE: // return
      --sp;
      pc = stack[sp];
      pc += 2;
      return 0;

    default: goto fail;
    }

  case 0x1000: // 0x1NNN: jump to address NNN
    pc = opcode & 0xFFF;
    return 0;

  case 0x2000: // 0x2NNN: call subroutine at NNN
    stack[sp] = pc;
    ++sp;
    pc = opcode & 0xFFF;
    return 0;

  case 0x3000: // 0x3XNN: skip next instruction if VX == NN
    if (V[(opcode & 0xF00) >> 8] == (opcode & 0xFF)) pc += 4;
    else pc += 2;
    return 0;

  case 0x4000: // 0x4XNN: skip next instruction if VX != NN
    if (V[(opcode & 0xF00) >> 8] != (opcode & 0xFF)) pc += 4;
    else pc += 2;
    return 0;

  case 0x5000: // 0x5XY0: skip next instruction if VX == VY
    if (V[(opcode & 0xF00) >> 8] != V[(opcode & 0xF0) >> 4]) pc += 4;
    else pc += 2;
    return 0;

  case 0x6000: // 0x6XNN: set VX to NN
    V[(opcode & 0xF00) >> 8] = opcode & 0xFF;
    pc += 2; return 0;

  case 0x7000: // 0x7XNN: add NN to VX
    V[(opcode & 0xF00) >> 8] += opcode & 0xFF;
    pc += 2; return 0;

  case 0x8000: // 0x8XY_ opcodes
    switch (opcode & 0xF) {
    case 0: // 0x8XY0: set VX to VY
      V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4];
      pc += 2; return 0;

    case 1: // 0x8XY1: set VX to VX | VY
      V[(opcode & 0xF00) >> 8] |= V[(opcode & 0xF0) >> 4];
      pc += 2; return 0;

    case 2: // 0x8XY2: set VX to VX & VY
      V[(opcode & 0xF00) >> 8] &= V[(opcode & 0xF0) >> 4];
      pc += 2; return 0;

    case 3: // 0x8XY3: set VX to VX ^ VY
      V[(opcode & 0xF00) >> 8] ^= V[(opcode & 0xF0) >> 4];
      pc += 2; return 0;

    case 4: // 0x8XY4: add VY to VX; set VF to 1 if there is a carry, 0 otherwise
      V[(opcode & 0xF00) >> 8] += V[(opcode & 0xF0) >> 4];
      if (V[(opcode & 0xF0) >> 4] > V[(opcode & 0xF00) >> 8])
        V[0xF] = 1;
      else V[0xF] = 0;
      pc += 2; return 0;

    case 5: // 0x8XY5: subtract VY from VX;
      // set VF to 0 if there is a borrow, 1 otherwise
      if (V[(opcode & 0xF0) >> 4] > V[(opcode & 0xF00) >> 8])
        V[0xF] = 0;
      else V[0xF] = 1;
      V[(opcode & 0xF00) >> 8] -= V[(opcode & 0xF0) >> 4];
      pc += 2; return 0;

    case 6: // 0x8XY6: shift VX right by 1; set VF to the least significant bit of
      // VX before shifting
      V[0xF] = V[(opcode & 0xF00) >> 8] & 1;
      V[(opcode & 0xF00) >> 8] >>= 1;
      pc += 2; return 0;

    case 7: // 0x8XY7: set VX to VY - VX;
      // set VF to 0 if there is a borrow, 1 otherwise
      if (V[(opcode & 0xF00) >> 8] > V[(opcode & 0xF0) >> 4])
        V[0xF] = 0;
      else V[0xF] = 1;
      V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4] - V[(opcode & 0xF00) >> 8];
      pc += 2; return 0;

    case 0xE: // 0x8XYE: shift VX left by 1; set VF to the mosteast significant bit of
      // VX before shifting
      V[0xF] = V[(opcode & 0xF00) >> 8] >> 7;
      V[(opcode & 0xF00) >> 8] <<= 1;
      pc += 2; return 0;

    default: goto fail;
    }

  default:
  fail:
    printf("Unknown opcode: %x\n", opcode);
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Usage: chip8 [filename]\n");
    return 1;
  }

  filename = argv[1];
  if (init()) return 1;
  SDL_Init(SDL_INIT_EVERYTHING);

  uint32_t w = 1024;
  uint32_t h = 512;
  uint32_t tmp_screen[SCREEN_SIZE];

  SDL_Window *window = SDL_CreateWindow(
    "chip8",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    w, h,
    SDL_WINDOW_SHOWN
    );

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  SDL_RenderSetLogicalSize(renderer, w, h);

  SDL_Texture *texture = SDL_CreateTexture(
    renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32
    );

  SDL_Event ev;
  while (1) {
    if (cycle()) return 1;

    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) return 0;
    }

    if (draw_flag) {
      draw_flag = 0;

      for (uint32_t i = 0; i < SCREEN_SIZE; ++i)
        tmp_screen[i] = (screen[i] * 0xFFFFFF) | 0xFF000000;

      SDL_UpdateTexture(texture, NULL, tmp_screen, 64 * sizeof(uint32_t));
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
    }

    usleep(1200);
  }
  return 0;
}
