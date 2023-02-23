#ifndef LEGILIMENS_HOGWARTS_LEGACY_CPP_COLLECTIBLES_H
#define LEGILIMENS_HOGWARTS_LEGACY_CPP_COLLECTIBLES_H

#include <string>
#include <vector>

enum CollectibleEnum {
    Revelio = 0,
    Flying = 1,
    Moth = 2,
    Brazier = 3,
    Statue = 4,
    DaedalianKey = 5,
    Demiguise = 6,
    Balloon = 7,
    Landing = 8,
    Merlin = 9,
    Astronomy = 10,
    AncientMagic = 11,
    Foe = 12,
    ButterflyChest = 13,
    VivariumChest = 14,
    MiscChest = 15,
    ArithmancyChest = 16,
    DungeonChest = 17,
    CampChest = 18
};

enum TableEnum {
    CollectionDynamic = 0,
    SphinxPuzzleDynamic = 1,
    LootDropComponentDynamic = 2,
    EconomicExpiryDynamic = 3,
    MiscDataDynamic = 4,
    MapLocationDataDynamic = 5
};

enum RegionEnum {
    Butterflies = 0,
    DaedalianKeys = 1,
    Hogsmeade = 2,
    TheAstronomyWing = 3,
    TheBellTowerWing = 4,
    TheGrandStaircase = 5,
    TheGreatHall = 6,
    TheLibraryAnnex = 7,
    TheSouthWing = 8,
    Vivariums = 9,
    ClagmarCoast = 10,
    CoastalCavern = 11,
    Cragcroftshire = 12,
    FeldcroftRegion = 13,
    ForbiddenForest = 14,
    HogsmeadeValley = 15,
    HogwartsValley = 16,
    ManorCape = 17,
    MarunweemLake = 18,
    NorthFordBog = 19,
    NorthHogwartsRegion = 20,
    PoidsearCoast = 21,
    SouthHogwartsRegion = 22,
    SouthSeaBog = 23
};

struct CollectibleStruct {
    CollectibleEnum type;
    std::string key;
    uint8_t video;
    uint16_t timestamp;
    RegionEnum region;
    uint8_t index;
};

// extern const std::string collectibleNames[];
extern const std::string timestampNames[];
extern const std::string timestampParen[];
extern const TableEnum collectibleTables[];
extern const std::vector<std::string> queries;
extern const std::string regionNames[];
extern const std::string globalRegionNames[];
extern const std::string videoIds[];
extern const std::vector<CollectibleStruct> collectibles;

#endif //LEGILIMENS_HOGWARTS_LEGACY_CPP_COLLECTIBLES_H