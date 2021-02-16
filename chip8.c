
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
  SDLK_x, SDLK_1, SDLK_2, SDLK_3,
  SDLK_q, SDLK_w, SDLK_e, SDLK_a,
  SDLK_s, SDLK_d, SDLK_z, SDLK_c,
  SDLK_4, SDLK_r, SDLK_f, SDLK_v
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

// emulate one cycle
uint8_t cycle()
{
  opcode = (memory[pc] << 8) | memory[pc + 1];
  switch (opcode & 0xF000) {
  case 0:
    switch (opcode & 0xF) {
    case 0: // 0x0000: clear screen
      memset(screen, 0, sizeof(screen));
      draw_flag = 1;
      pc += 2; break;

    case 0xE: // 0x000E: return
      --sp;
      pc = stack[sp];
      pc += 2; break;

    default: goto fail;
    }
    break;

  case 0x1000: // 0x1NNN: jump to address NNN
    pc = opcode & 0xFFF;
    break;

  case 0x2000: // 0x2NNN: call subroutine at NNN
    stack[sp] = pc;
    ++sp;
    pc = opcode & 0xFFF;
    break;

  case 0x3000: // 0x3XNN: skip next instruction if VX == NN
    if (V[(opcode & 0xF00) >> 8] == (opcode & 0xFF)) pc += 4;
    else pc += 2;
    break;

  case 0x4000: // 0x4XNN: skip next instruction if VX != NN
    if (V[(opcode & 0xF00) >> 8] != (opcode & 0xFF)) pc += 4;
    else pc += 2;
    break;

  case 0x5000: // 0x5XY0: skip next instruction if VX == VY
    if (V[(opcode & 0xF00) >> 8] == V[(opcode & 0xF0) >> 4]) pc += 4;
    else pc += 2;
    break;

  case 0x6000: // 0x6XNN: set VX to NN
    V[(opcode & 0xF00) >> 8] = opcode & 0xFF;
    pc += 2; break;

  case 0x7000: // 0x7XNN: add NN to VX
    V[(opcode & 0xF00) >> 8] += opcode & 0xFF;
    pc += 2; break;

  case 0x8000: // 0x8XY_ opcodes
    switch (opcode & 0xF) {
    case 0: // 0x8XY0: set VX to VY
      V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4];
      pc += 2; break;

    case 1: // 0x8XY1: set VX to VX | VY
      V[(opcode & 0xF00) >> 8] |= V[(opcode & 0xF0) >> 4];
      pc += 2; break;

    case 2: // 0x8XY2: set VX to VX & VY
      V[(opcode & 0xF00) >> 8] &= V[(opcode & 0xF0) >> 4];
      pc += 2; break;

    case 3: // 0x8XY3: set VX to VX ^ VY
      V[(opcode & 0xF00) >> 8] ^= V[(opcode & 0xF0) >> 4];
      pc += 2; break;

    case 4: // 0x8XY4: add VY to VX; set VF to 1 if there is a carry, 0 otherwise
      if ((uint16_t)V[(opcode & 0xF0) >> 4] + (uint16_t)V[(opcode & 0xF00) >> 8] > 0xFF)
        V[0xF] = 1;
      else V[0xF] = 0;
      V[(opcode & 0xF00) >> 8] += V[(opcode & 0xF0) >> 4];
      pc += 2; break;

    case 5: // 0x8XY5: subtract VY from VX;
      // set VF to 0 if there is a borrow, 1 otherwise
      if (V[(opcode & 0xF0) >> 4] > V[(opcode & 0xF00) >> 8])
        V[0xF] = 0;
      else V[0xF] = 1;
      V[(opcode & 0xF00) >> 8] -= V[(opcode & 0xF0) >> 4];
      pc += 2; break;

    case 6: // 0x8XY6: shift VX right by 1; set VF to the least significant bit of
      // VX before shifting
      V[0xF] = V[(opcode & 0xF00) >> 8] & 1;
      V[(opcode & 0xF00) >> 8] >>= 1;
      pc += 2; break;

    case 7: // 0x8XY7: set VX to VY - VX;
      // set VF to 0 if there is a borrow, 1 otherwise
      if (V[(opcode & 0xF00) >> 8] > V[(opcode & 0xF0) >> 4])
        V[0xF] = 0;
      else V[0xF] = 1;
      V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4] - V[(opcode & 0xF00) >> 8];
      pc += 2; break;

    case 0xE: // 0x8XYE: shift VX left by 1; set VF to the most significant bit of
      // VX before shifting
      V[0xF] = V[(opcode & 0xF00) >> 8] >> 7;
      V[(opcode & 0xF00) >> 8] <<= 1;
      pc += 2; break;

    default: goto fail;
    }
    break;

  case 0x9000: // 0x9XY0: skip next instruction if VX != VY
    if (V[(opcode & 0xF00) >> 8] != V[(opcode & 0xF0) >> 4]) pc += 4;
    else pc += 2;
    break;

  case 0xA000: // 0xANNN: set I to NNN
    I = opcode & 0xFFF;
    pc += 2; break;

  case 0xB000: // 0xBNNN: jump to address NNN + V0
    pc = (opcode & 0xFFF) + V[0];
    break;

  case 0xC000: // 0xCXNN: set VX to a random number masked by NN
    V[(opcode & 0xF00) >> 8] = (rand() & 0xFF) & (opcode & 0xFF);
    pc += 2; break;

  case 0xD000: // 0xDXYN: draw a sprite at coordinates (VX, VY);
    // sprite has a width of 8 pixels and a height of N pixels;
    // sprite is read from location I;
    // set VF to 1 if any pixels within the sprite were already on, 0 otherwise
  {
    uint16_t vx = V[(opcode & 0xF00) >> 8];
    uint16_t vy = V[(opcode & 0xF0) >> 4];
    uint16_t h = opcode & 0xF;

    V[0xF] = 0;
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < 8; ++x) {
        if (memory[I + y] & (0x80 >> x)) {
          uint8_t *pix = &(screen[vx + x + ((vy + y) * 64)]);
          if (*pix) V[0xF] = 1;
          *pix ^= 1;
        }
      }
    }

    draw_flag = 1;
    pc += 2; break;
  }

  case 0xE000:
    switch (opcode & 0xFF) {
    case 0x9E: // 0xEX9E: skip the next instruction if the key in VX is pressed
      if (keys[V[(opcode & 0xF00) >> 8]]) pc += 4;
      else pc += 2;
      break;

    case 0xA1: // 0xEXA1: skip the next instruction if the key in VX is not pressed
      if (keys[V[(opcode & 0xF00) >> 8]] == 0) pc += 4;
      else pc += 2;
      break;

    default: goto fail;
    }
    break;

  case 0xF000:
    switch (opcode & 0xFF) {
    case 7: // 0xFX07: set VX to the value of the delay timer
      V[(opcode & 0xF00) >> 8] = delay_timer;
      pc += 2; break;

    case 0xA: //0xFX0A: wait for a key press and then store it in VX
    {
      uint8_t key_pressed = 0;
      for (uint32_t i = 0; i < 16; ++i) {
        if (keys[i]) {
          V[(opcode & 0xF00) >> 8] = i;
          key_pressed = 1;
        }
      }
      if (key_pressed) { pc += 2; break; }
      else return 0;
    }

    case 0x15: // 0xFX15: set the delay timer to VX
      delay_timer = V[(opcode & 0xF00) >> 8];
      pc += 2; break;

    case 0x18: // 0xFX18: set the sound timer to VX
      sound_timer = V[(opcode & 0xF00) >> 8];
      pc += 2; break;

    case 0x1E: // 0xFX1E: add VX to I; set VF to 1 if there is an overflow,
      // 0 otherwise
      if ((uint32_t)I + (uint32_t)V[(opcode & 0xF00) >> 8] > 0xFFFF) V[0xF] = 1;
      else V[0xF] = 0;
      I += V[(opcode & 0xF00) >> 8];
      pc += 2; break;

    case 0x29: // 0xFX29: set I to the location of the sprite for the character
      // in VX; each character is a 4*5 pixel sprite
      I = V[(opcode & 0xF00) >> 8] * 5;
      pc += 2; break;

    case 0x33: // 0xFX33: convert VX to decimal and store its digits at addresses
      // I, I+1 and I+2
      memory[I] = V[(opcode & 0xF00) >> 8] / 100;
      memory[I + 1] = (V[(opcode & 0xF00) >> 8] / 10) % 10;
      memory[I + 2] = V[(opcode & 0xF00) >> 8] % 10;
      pc += 2; break;

    case 0x55: // 0xFX55: store V0 to VX in memory starting at address I
      for (uint32_t i = 0; i <= ((opcode & 0xF00) >> 8); ++i)
        memory[I + i] = V[i];
      pc += 2; break;

    case 0x65: // 0xFX65: load V0 to VX from memory starting at address I
      for (uint32_t i = 0; i <= ((opcode & 0xF00) >> 8); ++i)
        V[i] = memory[I + i];
      pc += 2; break;

    default: goto fail;
    }
    break;

  default:
  fail:
    printf("Unknown opcode: %x\n", opcode);
    return 1;
  }

  if (delay_timer) --delay_timer;
  if (sound_timer) --sound_timer;

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

  while (1) {
    if (cycle()) return 1;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) return 0;

      if (ev.type == SDL_KEYDOWN)
        for (uint32_t i = 0; i < 16; ++i)
          if (keymap[i] == ev.key.keysym.sym) keys[i] = 1;

      if (ev.type == SDL_KEYUP)
        for (uint32_t i = 0; i < 16; ++i)
          if (keymap[i] == ev.key.keysym.sym) keys[i] = 0;
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
