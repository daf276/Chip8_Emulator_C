#include <iostream>
#include <fstream>
#include "Chip8.h"
#include <random>
#include <algorithm>
#include <memory>

namespace Emulator {

    Chip8::Chip8() : memory(4096), v(16), stack(16), key_pressed(16), gfx(64*32), mt(new std::mt19937(std::random_device()())), dist(new std::uniform_real_distribution<double>(0.0, 255.0)){
        PopulateOpCodeTables();
        LoadHexDigitSpriteIntoMemory();

        program_counter = 0x200;
    }

    Chip8::Chip8(std::string path) : Chip8() {
        std::ifstream inputfile;
        inputfile.open(path.c_str());

        if (inputfile.is_open()) {
            int counter = 0x200;
            while (!inputfile.eof()) {
                inputfile >> memory.at(counter);
                counter++;
            }

            inputfile.close();
        } else std::cerr << std::string("Can't find inputfile") << path << std::endl;
    }

    void Chip8::EmulateCycle() {
        opcode = memory[program_counter] << 8 | memory[program_counter + 1];
        std::cout << std::hex << std::uppercase << opcode << std::endl;

        //Call function on opcode_table where the index equals the first hex digit of the opcode
        (this->*opcode_table[(opcode & 0xF000) >> 12])();

        // TODO Fully implement timers
        /*if (delay_timer > 0)
            --delay_timer;

        if (sound_timer > 0)
            if(sound_timer == 1);
        --sound_timer;*/
    }

    void Chip8::PopulateOpCodeTables() {
        opcode_table = {&Chip8::OpCodeZero, &Chip8::Jump, &Chip8::Call, &Chip8::RegisterAndConstantSE,
                        &Chip8::RegisterAndConstantSNE, &Chip8::TwoRegistersSE, &Chip8::LoadConstantIntoRegister,
                        &Chip8::AddConstantToRegister, &Chip8::OpCodeEight, &Chip8::TwoRegistersSNE,
                        &Chip8::SetRegisterIToConstant, &Chip8::JumpToConstantPlusV0, &Chip8::SetCpuRegisterRandom,
                        &Chip8::DisplaySprite, &Chip8::OpCodeE, &Chip8::OpCodeF};

        opcode8_table = {&Chip8::StoreRegisterYInX, &Chip8::ORRegisterXAndY, &Chip8::ANDRegisterXAndY,
                         &Chip8::XORRegisterXAndY, &Chip8::ADDRegisterXAndY, &Chip8::SUBRegisterXAndY,
                         &Chip8::SHRRegisterX, &Chip8::SUBNRegisterXAndY, &Chip8::OpCodeInvalid, &Chip8::OpCodeInvalid,
                         &Chip8::OpCodeInvalid, &Chip8::OpCodeInvalid, &Chip8::OpCodeInvalid, &Chip8::OpCodeInvalid,
                         &Chip8::SHLRegisterX, &Chip8::OpCodeInvalid};

        opcodeF_table = {&Chip8::OpCodeFx0x, &Chip8::OpCodeFx1x, &Chip8::LoadFontLocationIntoIndexRegister, &Chip8::OpCodeFx3x,
                         &Chip8::OpCodeInvalid, &Chip8::OpCodeFx5x, &Chip8::OpCodeFx6x};
    }

    void Chip8::OpCodeInvalid() {
        std::cerr << "Invalid opcode" << std::endl;
    }

    void Chip8::OpCodeZero() {//Opcodes 0XXX
        if (opcode == 0x00E0) { //CLS
            std::fill(gfx.begin(), gfx.end(), 0);
        } else if (opcode == 0x00EE) { //RET
            stack_pointer--;
            program_counter = stack[stack_pointer];
        }
        SetPCToNextInstruction();
    }

    void Chip8::Jump() { //Opcode 1XXX -> JP addr
        program_counter = opcode & 0x0FFF;
    }

    void Chip8::Call() { //Opcode 2XXX -> CALL addr
        stack[stack_pointer] = program_counter;
        stack_pointer++;
        program_counter = opcode & 0x0FFF;
    }

    void Chip8::RegisterAndConstantSE() { //Opcode 3XXX -> SE Vx, byte
        if (v[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
            SetPCToSkipNextInstruction();
        else
            SetPCToNextInstruction();
    }

    void Chip8::RegisterAndConstantSNE() { //Opcode 4XXX -> SNE Vx, byte
        if (v[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
            SetPCToSkipNextInstruction();
        else
            SetPCToNextInstruction();
    }

    void Chip8::TwoRegistersSE() { //Opcode 5XXX -> SE Vx, Vy
        if (v[(opcode & 0x0F00) >> 8] == v[(opcode & 0x00F0) >> 4])
            SetPCToSkipNextInstruction();
        else
            SetPCToNextInstruction();
    }

    void Chip8::LoadConstantIntoRegister() { //Opcode 6XXX -> LD Vx, byte
        v[(opcode & 0x0F00) >> 8] = opcode & 0x00FF; //Casting gives the 2 LSBs, aka the constant
        SetPCToNextInstruction();
    }

    void Chip8::AddConstantToRegister() { //Opcode 7XXX -> ADD Vx, byte
        v[(opcode & 0x0F00) >> 8] += opcode & 0x00FF; //Casting gives the 2 LSBs, aka the constant
        SetPCToNextInstruction();
    }

    void Chip8::OpCodeEight() { //All Opcodes in the form of 8XXX
        (this->*opcode8_table[opcode & 0x000F])();
        SetPCToNextInstruction();
    }

    void Chip8::StoreRegisterYInX() { //opcode 8XX0 -> LD Vx, Vy
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x00F0) >> 4];
    }

    void Chip8::ORRegisterXAndY() { //opcode 8XX1 -> OR Vx, Vy
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x0F00) >> 8] | v[(opcode & 0x00F0) >> 4];
    }

    void Chip8::ANDRegisterXAndY() { //opcode 8XX2 -> AND Vx, Vy
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x0F00) >> 8] & v[(opcode & 0x00F0) >> 4];
    }

    void Chip8::XORRegisterXAndY() { //opcode 8XX3 -> XOR Vx, Vy
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x0F00) >> 8] ^ v[(opcode & 0x00F0) >> 4];
    }

    void Chip8::ADDRegisterXAndY() { //opcode 8XX4 -> AND Vx, Vy
        v[15] = (v[(opcode & 0x0F00) >> 8] + v[(opcode & 0x00F0) >> 4] > 255) ? 1 : 0;
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x0F00) >> 8] + v[(opcode & 0x00F0) >> 4];
    }

    void Chip8::SUBRegisterXAndY() { //opcode 8XX5 -> SUB Vx, Vy
        v[15] = (v[(opcode & 0x0F00) >> 8] > v[(opcode & 0x00F0) >> 4]) ? 1 : 0;
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x0F00) >> 8] - v[(opcode & 0x00F0) >> 4];
    }

    void Chip8::SHRRegisterX() { //opcode 8XX6 -> SHR Vx {, Vy}
        v[15] = v[(opcode & 0x0F00) >> 8] & 0b1;
        v[(opcode & 0x0F00) >> 8] /= 2;
    }

    void Chip8::SUBNRegisterXAndY() { //opcode 8XX7 -> SUBN Vx, Vy
        v[15] = v[(opcode & 0x00F0) >> 4] > (v[(opcode & 0x0F00) >> 8]) ? 1 : 0;
        v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x00F0) >> 4] - v[(opcode & 0x0F00) >> 8];
    }

    void Chip8::SHLRegisterX() { //opcode 8XXE -> SHL Vx {, Vy}
        v[15] = (v[(opcode & 0x0F00) >> 8] & 0b10000000) >> 7;
        v[(opcode & 0x0F00) >> 8] *= 2;
    }

    void Chip8::TwoRegistersSNE() { //Opcode 9XXX -> SNE Vx, Vy
        if (v[(opcode & 0x0F00) >> 8] != v[(opcode & 0x00F0) >> 4])
            SetPCToSkipNextInstruction();
        else
            SetPCToNextInstruction();
    }

    void Chip8::SetRegisterIToConstant() { //Opcode AXXX -> LD I, addr
        index_register = opcode & 0x0FFF;
        SetPCToNextInstruction();
    }

    void Chip8::JumpToConstantPlusV0() { //Opcode BXXX -> JP V0, addr
        program_counter = (opcode & 0x0FFF) + v[0];
    }

    void Chip8::SetCpuRegisterRandom() { //Opcode CXXX -> RND Vx, byte
        v[(opcode & 0x0F00) >> 8] =
                static_cast<unsigned char>((0x00FF & opcode)) & static_cast<unsigned char>(dist->operator()(*mt));
        SetPCToNextInstruction();
    }

    void Chip8::DisplaySprite() { //Opcode DXXX -> DRW Vx, Vy, nibble

        unsigned char number_of_bytes = static_cast<unsigned char>(opcode & 0x000F);
        unsigned char x = v[(opcode & 0x0F00) >> 8];
        unsigned char y = v[(opcode & 0x00F0) >> 4];

        for (int i = 0; i < number_of_bytes; ++i) {
            unsigned char row_of_pixels = memory[index_register + i];

            gfx[64 * (y + i) + x] = ((row_of_pixels & 128) > 0) ^ gfx[64 * (y + i) + x];
            gfx[64 * (y + i) + x + 1] = ((row_of_pixels & 64) > 0) ^ gfx[64 * (y + i) + x + 1];
            gfx[64 * (y + i) + x + 2] = ((row_of_pixels & 32) > 0) ^ gfx[64 * (y + i) + x + 2];
            gfx[64 * (y + i) + x + 3] = ((row_of_pixels & 16) > 0) ^ gfx[64 * (y + i) + x + 3];
            gfx[64 * (y + i) + x + 4] = ((row_of_pixels & 8) > 0) ^ gfx[64 * (y + i) + x + 4];
            gfx[64 * (y + i) + x + 5] = ((row_of_pixels & 4) > 0) ^ gfx[64 * (y + i) + x + 5];
            gfx[64 * (y + i) + x + 6] = ((row_of_pixels & 2) > 0) ^ gfx[64 * (y + i) + x + 6];
            gfx[64 * (y + i) + x + 7] = ((row_of_pixels & 1) > 0) ^ gfx[64 * (y + i) + x + 7];
        }

        //std::cout << static_cast<int>(y) << std::endl;
        for (int j = 0; j < 64*32; ++j) {
            //if(gfx[j]) display_pixels[j] = 255;
            //else display_pixels[j] = 0;
        }
        SetPCToNextInstruction();
    }

    //TODO actually implement key presses
    void Chip8::OpCodeE() { //Skip next instruction depending on key state
        if ((opcode & 0x00FF) == 0x009E) { //SKP Vx
            if (key_pressed[(opcode & 0x0F00) >> 8]) SetPCToSkipNextInstruction();
            else SetPCToNextInstruction();
        } else if ((opcode & 0x00FF) == 0x00A1) { //SKNP Vx
            if (!key_pressed[(opcode & 0x0F00) >> 8]) SetPCToSkipNextInstruction();
            else SetPCToNextInstruction();
        }
    }

    void Chip8::OpCodeF() {
        (this->*opcodeF_table[(opcode & 0x00F0) >> 4])();
    }

    void Chip8::OpCodeFx0x() {
        if ((opcode & 0x000F) == 0x7) { //LD Vx, DT
            v[(opcode & 0x0F00) >> 8] = delay_timer;
            SetPCToNextInstruction();
        } else if ((opcode & 0x000F) == 0xA) { //LD Vx, K
            if (std::any_of(key_pressed.begin(), key_pressed.end(), [](bool i) { return i; })) {
                SetPCToNextInstruction();
            }
        }
    }

    void Chip8::OpCodeFx1x() {
        if ((opcode & 0x000F) == 0x5) { //LD DT, Vx
            delay_timer = v[(opcode & 0x0F00) >> 8];
        } else if ((opcode & 0x000F) == 0x8) { //LD ST, Vx
            sound_timer = v[(opcode & 0x0F00) >> 8];
        } else if ((opcode & 0x000F) == 0xE) { //ADD I, Vx
            index_register += v[(opcode & 0x0F00) >> 8];
        }
        SetPCToNextInstruction();
    }

    void Chip8::LoadFontLocationIntoIndexRegister() { //LD F, Vx
        index_register = ((opcode & 0x0F00) >> 8) * 5;
        SetPCToNextInstruction();
    }

    void Chip8::OpCodeFx3x() {

    }

    void Chip8::OpCodeFx5x() {

    }

    void Chip8::OpCodeFx6x() {

    }

    void Chip8::SetPCToNextInstruction() {
        program_counter += 2;
    }

    void Chip8::SetPCToSkipNextInstruction() {
        program_counter += 4;
    }

    void Chip8::LoadHexDigitSpriteIntoMemory() {

        //Zero
        memory[0] = 0xF0;
        memory[1] = 0x90;
        memory[2] = 0x90;
        memory[3] = 0x90;
        memory[4] = 0xF0;

        //One
        memory[5] = 0x20;
        memory[6] = 0x60;
        memory[7] = 0x20;
        memory[8] = 0x20;
        memory[9] = 0x70;

        //Two
        memory[10] = 0xF0;
        memory[11] = 0x10;
        memory[12] = 0xF0;
        memory[13] = 0x80;
        memory[14] = 0xF0;

        //Three
        memory[15] = 0xF0;
        memory[16] = 0x10;
        memory[17] = 0xF0;
        memory[18] = 0x10;
        memory[19] = 0xF0;

        //Four
        memory[20] = 0x90;
        memory[21] = 0x90;
        memory[22] = 0xF0;
        memory[23] = 0x10;
        memory[24] = 0x10;

        //Five
        memory[25] = 0xF0;
        memory[26] = 0x80;
        memory[27] = 0xF0;
        memory[28] = 0x10;
        memory[29] = 0xF0;

        //Six
        memory[30] = 0xF0;
        memory[31] = 0x80;
        memory[32] = 0xF0;
        memory[33] = 0x90;
        memory[34] = 0xF0;

        //Seven
        memory[35] = 0xF0;
        memory[36] = 0x10;
        memory[37] = 0x20;
        memory[38] = 0x40;
        memory[39] = 0x40;

        //Eight
        memory[40] = 0xF0;
        memory[41] = 0x90;
        memory[42] = 0xF0;
        memory[43] = 0x90;
        memory[44] = 0xF0;

        //Nine
        memory[45] = 0xF0;
        memory[46] = 0x90;
        memory[47] = 0xF0;
        memory[48] = 0x10;
        memory[49] = 0xF0;

        //A
        memory[50] = 0xF0;
        memory[51] = 0x90;
        memory[52] = 0xF0;
        memory[53] = 0x90;
        memory[54] = 0x90;

        //B
        memory[55] = 0xE0;
        memory[56] = 0x90;
        memory[57] = 0xE0;
        memory[58] = 0x90;
        memory[59] = 0xE0;

        //C
        memory[60] = 0xF0;
        memory[61] = 0x80;
        memory[62] = 0x80;
        memory[63] = 0x80;
        memory[64] = 0xF0;

        //D
        memory[65] = 0xE0;
        memory[66] = 0x90;
        memory[67] = 0x90;
        memory[68] = 0x90;
        memory[69] = 0xE0;

        //E
        memory[70] = 0xF0;
        memory[71] = 0x80;
        memory[72] = 0xF0;
        memory[73] = 0x80;
        memory[74] = 0xF0;

        //F
        memory[75] = 0xF0;
        memory[76] = 0x80;
        memory[77] = 0xF0;
        memory[78] = 0x80;
        memory[79] = 0x80;
    }

    void Chip8::SetCpuRegister(int index, unsigned char value) {
        v[index] = value;
    }

    unsigned char Chip8::GetCpuRegister(int i) {
        return v[i];
    }

    unsigned short Chip8::GetStack(int i) {
        return stack[i];
    }

    void Chip8::WriteToMemory(int index, unsigned char value) {
        memory[index] = value;
    }

    unsigned short Chip8::GetIndexRegister() const {
        return index_register;
    }

    void Chip8::SetIndexRegister(unsigned short index_register) {
        Chip8::index_register = index_register;
    }

    unsigned short Chip8::GetProgramCounter() const {
        return program_counter;
    }

    void Chip8::SetProgramCounter(unsigned short program_counter) {
        Chip8::program_counter = program_counter;
    }

    unsigned char Chip8::GetStackPointer() const {
        return stack_pointer;
    }

    void Chip8::SetStackPointer(unsigned char stack_pointer) {
        Chip8::stack_pointer = stack_pointer;
    }

    unsigned char Chip8::GetDelayTimer() const {
        return delay_timer;
    }

    void Chip8::SetDelayTimer(unsigned char delay_timer) {
        Chip8::delay_timer = delay_timer;
    }

    unsigned char Chip8::GetSoundTimer() const {
        return sound_timer;
    }

    void Chip8::SetSoundTimer(unsigned char sound_timer) {
        Chip8::sound_timer = sound_timer;
    }
}