#include <Client/StaticData.hh>

uint32_t const RARITY_COLORS[RarityID::kNumRarities] = { 
    0xff7eef6d, 0xffffe65d, 0xff4d52e3, 
    0xff861fde, 0xffde1f1f, 0xff1fdbde/* 0xffbe98aa */,
    0xffde1f65
 }; // 0xffff2b75, 0xfff70fb6};

uint32_t const FLOWER_COLORS[ColorID::kNumColors] = {
    0xffffe763, 0xff999999, 0xff789cf5, 0xffec6869
};

char const *RARITY_NAMES[RarityID::kNumRarities] = {
    "Common",
    "Unusual",
    "Rare",
    "Epic",
    "Legendary",
    "Mythic",
    "Unique"
};

char const SLOT_KEYBINDS[MAX_SLOT_COUNT] = 
    {'1','2','3','4','5','6','7','8','9'};
uint8_t const SLOT_KEYCODES[MAX_SLOT_COUNT] = 
    {'1','2','3','4','5','6','7','8','9'};