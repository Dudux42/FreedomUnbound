#pragma once
#include <cstdint>
namespace portablekit::fu::monster_hp {
// USA species IDs (the actor's +0x1E8 byte). An explicit large-monster list
// keeps prey, insects and Lynians out, regardless of their quest HP scaling.
inline const char *large_monster_name(unsigned species) {
    switch (species) {
    case 1: return "Rathian"; case 2: return "Fatalis";
    case 6: return "Yian Kut-Ku"; case 7: return "Lao-Shan Lung";
    case 8: return "Cephadrome"; case 11: return "Rathalos";
    case 14: return "Diablos"; case 15: return "Khezu";
    case 17: return "Gravios"; case 20: return "Gypceros";
    case 21: return "Plesioth"; case 22: return "Basarios";
    case 26: return "Monoblos"; case 27: return "Velocidrome";
    case 28: return "Gendrome"; case 31: return "Iodrome";
    case 33: return "Kirin"; case 36: return "Crimson Fatalis";
    case 37: return "Pink Rathian"; case 38: return "Blue Yian Kut-Ku";
    case 39: return "Purple Gypceros"; case 40: return "Yian Garuga";
    case 41: return "Silver Rathalos"; case 42: return "Gold Rathian";
    case 43: return "Black Diablos"; case 44: return "White Monoblos";
    case 45: return "Red Khezu"; case 46: return "Green Plesioth";
    case 47: return "Black Gravios"; case 48: return "Daimyo Hermitaur";
    case 49: return "Azure Rathalos"; case 50: return "Ash Lao-Shan Lung";
    case 51: return "Blangonga"; case 52: return "Congalala";
    case 53: return "Rajang"; case 54: return "Kushala Daora";
    case 55: return "Shen Gaoren"; case 58: return "Yama Tsukami";
    case 59: return "Chameleos"; case 60: return "Rusted Kushala Daora";
    case 64: return "Lunastra"; case 65: return "Teostra";
    case 67: return "Shogun Ceanataur"; case 68: return "Bulldrome";
    case 71: return "White Fatalis"; case 75: return "Tigrex";
    case 76: return "Akantor"; case 77: return "Giadrome";
    case 78: return "Yian Garuga"; case 79: return "King Shakalaka";
    case 80: return "Vespoid Queen"; case 81: return "Nargacuga";
    case 82: return "Hypnocatrice"; case 83: return "Lavasioth";
    case 84: return "Copper Blangonga"; case 85: return "Emerald Congalala";
    case 86: return "Plum Daimyo Hermitaur"; case 87: return "Terra Shogun Ceanataur";
    case 88: return "Ukanlos"; case 89: return "Rajang";
    default: return nullptr;
    }
}
}
