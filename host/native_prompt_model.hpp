#pragma once
#include "input/bindings.hpp"
#include <optional>
#include <string>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>
namespace portablekit::fu::controller_prompts {
// Regions measured from the original USA common-menu and heading atlases.
inline std::optional<input::Action> native_action(unsigned atlas,float u0,float v0,float u1,float v1) {
    if(!std::isfinite(u0+v0+u1+v1))return {};
    if(u1<u0)std::swap(u0,u1);if(v1<v0)std::swap(v0,v1);
    if(v1-v0>=6 && u1-u0>=6) {
        if(atlas==0 && u0>=64 && u1<=80 && v0>=16 && v1<=32)return input::Action::Up;
    }
    if(atlas==0 && v0>=16 && v1<=24 && v1-v0>=6 && u1-u0>=12) {
        if(u0>=0 && u1<=20)return input::Action::Start;
        if(u0>=20 && u1<=40)return input::Action::Select;
    }
    const float x=atlas==1 ? 104.f : 0.f,y=atlas==1 ? 144.f : 0.f;
    if(v0<y || v1>y+16 || u1-u0<6 || v1-v0<6)return {};
    static constexpr float starts[]{0,16,32,48,64,84,104},ends[]{16,32,48,64,84,104,120};
    static constexpr input::Action actions[]{input::Action::Circle,input::Action::Cross,input::Action::Triangle,
        input::Action::Square,input::Action::L,input::Action::R,input::Action::StickUp};
    for(unsigned cell=0;cell<7;++cell)if(u0>=x+starts[cell] && u1<=x+ends[cell])return actions[cell];
    return {};
}
inline std::string compact_key(std::string label) {
    for(const auto &[full,shorter]: {std::pair{"Mouse Left","ML"},{"Mouse Right","MR"},{"Mouse Middle","MM"},
        {"Mouse 4","M4"},{"Mouse 5","M5"},{"Left Shift","LS"},{"Right Shift","RS"},
        {"Left Ctrl","LC"},{"Right Ctrl","RC"},{"Left Alt","LA"},{"Right Alt","RA"},
        {"Enter","ENT"},{"Backspace","BKS"},{"Space","SPC"},{"Down","DN"},{"Left","LT"},{"Right","RT"},
        {"Left GUI","LG"},{"Right GUI","RG"},{"PageUp","PGU"},{"PageDown","PGD"},
        {"Keypad Enter","KEN"},{"Keypad Divide","K/"},{"Keypad Multiply","K*"},{"Keypad Minus","K-"},
        {"Keypad Plus","K+"},{"Keypad Period","K."},{"Numlock","NUM"},{"PrintScreen","PRT"},
        {"NonUSBackslash","\\"}})
        if(label==full)return shorter;
    if(label.starts_with("Keypad "))label="K"+label.substr(7);
    if(label.starts_with("Key "))label=label.substr(4);
    std::transform(label.begin(),label.end(),label.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    if(label.size()>3)label.resize(3);return label;
}
// Original 3x5 letterforms for compact key caps (row-major, high bit first).
inline unsigned key_pixels(char c) {
    switch(c) {
    case 'A':return 0b010101111101101;case 'B':return 0b110101110101110;
    case 'C':return 0b011100100100011;case 'D':return 0b110101101101110;
    case 'E':return 0b111100110100111;case 'F':return 0b111100110100100;
    case 'G':return 0b011100101101011;case 'H':return 0b101101111101101;
    case 'I':return 0b111010010010111;case 'J':return 0b001001001101010;
    case 'K':return 0b101101110101101;case 'L':return 0b100100100100111;
    case 'M':return 0b101111111101101;case 'N':return 0b101111111111101;
    case 'O':return 0b010101101101010;case 'P':return 0b110101110100100;
    case 'Q':return 0b010101101111011;case 'R':return 0b110101110101101;
    case 'S':return 0b011100010001110;case 'T':return 0b111010010010010;
    case 'U':return 0b101101101101111;case 'V':return 0b101101101101010;
    case 'W':return 0b101101111111101;case 'X':return 0b101101010101101;
    case 'Y':return 0b101101010010010;case 'Z':return 0b111001010100111;
    case '0':return 0b111101101101111;case '1':return 0b010110010010111;
    case '2':return 0b110001010100111;case '3':return 0b110001010001110;
    case '4':return 0b101101111001001;case '5':return 0b111100110001110;
    case '6':return 0b011100111101111;case '7':return 0b111001010010010;
    case '8':return 0b111101111101111;case '9':return 0b111101111001110;
    case '+':return 0b000010111010000;case '-':return 0b000000111000000;
    case '.':return 0b000000000000010;case ',':return 0b000000000010100;
    case '/':return 0b001001010100100;case '\\':return 0b100100010001001;
    case '[':return 0b110100100100110;case ']':return 0b011001001001011;
    case ';':return 0b000010000010100;case '\'':return 0b010010000000000;
    case '`':return 0b100010000000000;case '=':return 0b000111000111000;
    case '*':return 0b000101010101000;case '#':return 0b101111101111101;
    default:return 0b111001010000010;
    }
}
}
