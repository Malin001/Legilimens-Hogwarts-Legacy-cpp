#include <iostream>
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <map>
#include <regex>
#include "sqlite3.h"
#include "collectibles.h"
#include "getsave.h"
#include "tabulate.hpp"
#include "argparse.hpp"
#include "color.hpp"

#define DB_IMAGE_STR "RawDatabaseImage"
#define VERSION "0.2.3"
#define DEFAULT_OUTPUT_FILE "legilimens-output-{TIMESTAMP}.txt"

// Writes title to stream
void printTitle(std::ostream &stream) {
    tabulate::Table table;
    table.format().font_align(tabulate::FontAlign::center).hide_border_top().width(TABLE_WIDTH);
    table.add_row({R"(|                _                _ _ _                                        |)"});
    table.add_row({R"(|               | |              (_) (_)                                       |)"});
    table.add_row({R"(|               | |     ___  __ _ _| |_ _ __ ___   ___ _ __  ___               |)"});
    table.add_row({R"(|               | |    / _ \/ _` | | | | '_ ` _ \ / _ \ '_ \/ __|              |)"});
    table.add_row({R"(|               | |___|  __/ (_| | | | | | | | | |  __/ | | \__ \              |)"});
    table.add_row({R"(|               |______\___|\__, |_|_|_|_| |_| |_|\___|_| |_|___/              |)"});
    table.add_row({R"(|                            __/ |                                             |)"});
    table.add_row({R"(|                           |___/                                              |)"});
    table.add_row({"A Hogwarts Legacy tool to find your missing collectibles"});
    table.add_row({"https://github.com/Malin001/Legilimens-Hogwarts-Legacy-cpp"});
    std::string versionStr = "Version ";
    versionStr += VERSION;
    versionStr += " - Created by Malin";
    table.add_row({versionStr});
    table.row(0).format().show_border_top();
    for (int i = 0; i < 8; i++) table.row(i).format().padding_left(0).padding_right(0);
    table.row(8).format().show_border_top();
    stream << table << std::endl;
}

// Parses command line arguments
argparse::ArgumentParser parseArgs(int argc, char *argv[], bool &success) {
    argparse::ArgumentParser program("Legilimens", VERSION);
    program.add_argument("file").default_value(std::string{""}).help("Path of your .sav Hogwarts Legacy save file. Will be prompted if empty");
    program.add_argument("--dont-confirm-exit").default_value(false).implicit_value(true).help("Gets rid of the \"Press enter to close this window...\" prompt");
    program.add_argument("-o", "--output-file").default_value(std::string{DEFAULT_OUTPUT_FILE}).nargs(argparse::nargs_pattern::optional).help("File to write output to. To not write to file, use -o without passing a filename");
    std::string filters;
    for ( const auto &filter : filterOptions ) {
        filters += filter.cli + ", ";
    }
    program.add_argument("--filters").nargs(argparse::nargs_pattern::any).help("Only show certain collectibles. Will be prompted if empty. Can any combination of " + filters.substr(0, filters.length()-2));
    program.add_epilog("Example: Legilimens.exe C:/path/to/HL-00-00.sav --filters PAGES DAEDALIAN CHESTS");
    try {
        program.parse_args(argc, argv);
        success = true;
    } catch (const std::runtime_error& err) {
        std::cerr << dye::red(err.what()) << std::endl;
        std::cerr << dye::red(program);
        success = false;
    }
    return program;
}

// Gets the first available file path temp_X.db to temporarily store the database, return whether it was successful
bool getTempDBFile(const std::filesystem::path &exePath, std::filesystem::path &dbFile) {
    for (int i = 0; i < 1000; i++) {
        dbFile = exePath.parent_path() / ("temp_" + std::to_string(i) + ".db");
        if (!std::filesystem::exists(dbFile)) return true;
    }
    std::cerr << dye::red("Legilimens was unable to create a temporary database file") << std::endl;
    return false;
}

// Sets dbData to the content of the database contained in the saveFile, and returns whether it was successful
bool extractDB(const std::filesystem::path &saveFile, std::string &dbData) {
    // Check file existence
    if (!std::filesystem::exists(saveFile)) {
        std::cerr << dye::red("Legilimens was not able to find the file \"" + saveFile.string() + "\"") << std::endl;
        return false;
    }
    // Check for other errors
    std::ifstream fs(saveFile, std::ios::in|std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << dye::red("Legilimens encountered an error reading the file \"" + saveFile.string() + "\"") << std::endl;
        return false;
    }
    // Read the file
    std::ostringstream sstr;
    sstr << fs.rdbuf();
    std::string saveData = sstr.str();
    fs.close();
    // Check magic header
    if (!saveData.starts_with(MAGIC_HEADER)) {
        std::cerr << dye::red("File \"" + saveFile.string() + "\" doesn't seem to be a Hogwarts Legacy save file") << std::endl;
        return false;
    }
    // Find DB offset and size
    std::size_t found = saveData.find(DB_IMAGE_STR);
    if (found == std::string::npos || found + 65 >= saveData.length()) {
        std::cerr << dye::red("Legilimens was unable to find the SQL database in your save file") << std::endl;
        return false;
    }
    unsigned long long dbStartIndex = found + 65;
    unsigned int dbSize = readU32(saveData, dbStartIndex-4);
    // Extract DB
    dbData = saveData.substr(dbStartIndex, dbSize);
    return true;
}

// Runs a query
void runQuery(sqlite3* db, sqlite3_stmt* stmt, int index, std::vector<std::unordered_set<std::string>> &queryResults, std::unordered_set<TableEnum> &queryErrors) {
    try {
        if (sqlite3_prepare_v2(db, tables[index].query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            queryErrors.insert(TableEnum(index));
        } else if (!tables[index].oneRow) {
            while (sqlite3_step(stmt) != SQLITE_DONE) {
                queryResults[index].insert(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
            }
        } else {
            // Each row is a comma separated list of entries rather than one entry
            std::regex re("\\w+");
            std::string commaSepList;
            while (sqlite3_step(stmt) != SQLITE_DONE) {
                commaSepList = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                for (std::sregex_iterator i = std::sregex_iterator(commaSepList.begin(), commaSepList.end(), re); i != std::sregex_iterator(); i++) {
                    queryResults[index].insert(i->str());
                }
            }
        }
    } catch (const std::logic_error& sqlErr) {
        queryErrors.insert(TableEnum(index));
    }
    sqlite3_finalize(stmt);
}

// Read the tables in the database, and returns whether it was successful
bool readDB(const std::filesystem::path &saveFile, const std::filesystem::path &dbFile, std::vector<std::unordered_set<std::string>> &queryResults, std::unordered_set<TableEnum> &queryErrors) {
    // Create a new file containing the database
    std::string dbData;
    if (!extractDB(saveFile, dbData)) return false;
    std::ofstream fs(dbFile.string(), std::ios::out|std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << dye::red("Legilimens was unable to write the database to a new file") << std::endl;
        return false;
    }
    fs << dbData;
    fs.close();
    // Connect with sqlite3
    sqlite3* db;
    sqlite3_stmt* stmt;
    int err = sqlite3_open(dbFile.string().c_str(), &db);
    if (err == SQLITE_OK) {
        // Run each query
        for (int i = 0; i < tables.size(); i++) {
            runQuery(db, stmt, i, queryResults, queryErrors);
        }
    } else {
        std::cerr << dye::red("SQLite was unable to read the database") << std::endl;
    }
    if (queryErrors.size() == tables.size()) {
        std::cerr << dye::red("SQLite was unable to read the database") << std::endl;
        err = SQLITE_ERROR;
    }
    sqlite3_close(db);
    // Remove database file
    std::error_code ec;
    if (std::filesystem::exists(dbFile) && !std::filesystem::remove(dbFile, ec)) {
        std::cerr << dye::red("Error removing database file \"" + dbFile.string() + "\"") << std::endl;
        std::cerr << dye::red(ec.message()) << std::endl;
    }
    return (err == SQLITE_OK);
}

// Adds a row to the table for the given collectible when sorting by region
void addRegionTableRow(tabulate::Table &table, const CollectibleStruct& collectible) {
    CollectibleType type = collectibleTypes[collectible.type];
    std::string name = (collectible.type == FinishingTouchEnemy) ? collectible.index : type.timestampName + " #" + collectible.index;
    std::string video = "No video yet";
    if (collectible.video != UINT8_MAX) {
        video = "https://youtu.be/" + videoIds[collectible.video] + "&t=" + std::to_string(collectible.timestamp);
    }
    table.add_row({name, type.timeStampParen, video});
    tabulate::Color color = tabulate::Color::white;
    if (type.timestampName == "Field guide page") {
        color = tabulate::Color::cyan;
    } else if (type.timestampName == "Demiguise Moon") {
        color = tabulate::Color::blue;
    } else if (type.timestampName == "Merlin Trial") {
        color = tabulate::Color::green;
    } else if (type.timestampName == "Butterfly Chest") {
        color = tabulate::Color::yellow;
    } else if (type.timestampName == "Collection Chest") {
        color = tabulate::Color::magenta;
    }
    table[table.size()-1].format().font_color(color);
}

// Gets a header table for the given region
tabulate::Table getRegionHeaderTable(RegionEnum region) {
    tabulate::Table table;
    RegionStruct regionInfo = regions[region];
    if (regionInfo.globalRegion.empty()) {
        table.add_row({regionInfo.name});
    } else {
        table.add_row({regionInfo.globalRegion + " - " + regionInfo.name});
    }
    table[0].format().font_align(tabulate::FontAlign::center).hide_border().width(TABLE_WIDTH).padding_bottom(0);
    return table;
}

// Gets a table for the given region
tabulate::Table getRegionTable() {
    tabulate::Table table;
    table.add_row({"Item", "Type", "Location"});
    table.column(0).format().width(27);
    table.column(1).format().width(TABLE_WIDTH - 64 - 2);
    table.column(2).format().width(37);
    return table;
}

// Adds a row to the table for the given collectible when sorting by type
void addTypeTableRow(tabulate::Table &table, const CollectibleStruct& collectible) {
    CollectibleType type = collectibleTypes[collectible.type];
    RegionStruct regionInfo = regions[collectible.region];
    std::string name = (collectible.type == FinishingTouchEnemy) ? collectible.index : regionInfo.name + " #" + collectible.index;
    std::string video = "No video yet";
    if (collectible.video != UINT8_MAX) {
        video = "https://youtu.be/" + videoIds[collectible.video] + "&t=" + std::to_string(collectible.timestamp);
    }
    table.add_row({name, video});
    tabulate::Color color = tabulate::Color::white;
    if (type.timestampName == "Field guide page") {
        color = tabulate::Color::cyan;
    } else if (type.timestampName == "Demiguise Moon") {
        color = tabulate::Color::blue;
    } else if (type.timestampName == "Merlin Trial") {
        color = tabulate::Color::green;
    } else if (type.timestampName == "Butterfly Chest") {
        color = tabulate::Color::yellow;
    } else if (type.timestampName == "Collection Chest") {
        color = tabulate::Color::magenta;
    }
    table[table.size()-1].format().font_color(color);
}

// Gets a header table for the given collectible type
tabulate::Table getTypeHeaderTable(CollectibleEnum type) {
    tabulate::Table table;
    CollectibleType collectibleInfo = collectibleTypes[type];
    if (!collectibleInfo.sortByTypeName.empty()) {
        table.add_row({collectibleInfo.sortByTypeName});
    } else if (collectibleInfo.timeStampParen.empty()) {
        table.add_row({collectibleInfo.timestampName});
    } else {
        table.add_row({collectibleInfo.timestampName + " - " + collectibleInfo.timeStampParen});
    }
    table[0].format().font_align(tabulate::FontAlign::center).hide_border().width(TABLE_WIDTH).padding_bottom(0);
    return table;
}

// Gets a table for the given collectible type
tabulate::Table getTypeTable() {
    tabulate::Table table;
    table.add_row({"Item", "Location"});
    table.column(0).format().width(TABLE_WIDTH - 37 - 1);
    table.column(1).format().width(37);
    return table;
}

// Adds the given filter's types to the allowed types
void addFilterTypes(const Filter& filter, std::unordered_set<CollectibleEnum> &allowedTypes, bool &sortByType) {
    if (filter.cli == "ALL") {
        for (int i = 0; i < collectibleTypes.size(); i++) allowedTypes.insert(CollectibleEnum(i));
    } else if (filter.cli == "SORTTYPE") {
        sortByType = true;
    } else {
        for ( const auto &collectibleType : filter.types ) allowedTypes.insert(collectibleType);
    }
}

// Gets the list of filters to use, returns true if sorting by type instead of location
bool getFilters(const std::vector<std::string> &filters, std::unordered_set<CollectibleEnum> &allowedTypes) {
    bool sortByType = false;
    // Check command line args
    for ( const auto &filter : filters ) {
        for ( const auto &option : filterOptions ) {
            if (filter == option.cli) {
                addFilterTypes(option, allowedTypes, sortByType);
                break;
            }
        }
    }
    if (!allowedTypes.empty()) return sortByType;
    // Prompt user
    tabulate::Table table;
    table.add_row({"Choice", "Type", "", "Choice", "Type"});
    unsigned offset = (filterOptions.size() + 1) / 2;
    for (int i = 0; i < offset; i++) {
        if (i + offset < filterOptions.size()) {
            table.add_row({std::to_string(i), filterOptions[i].name, "", std::to_string(i+offset), filterOptions[i+offset].name});
        } else {
            table.add_row({std::to_string(i), filterOptions[i].name, "", "", ""});
        }
    }
    int width = (TABLE_WIDTH - 2*CHOICE_COL_WIDTH - 4) / 2;
    table.column(0).format().width(CHOICE_COL_WIDTH).font_align(tabulate::FontAlign::center);
    table.column(1).format().width(width);
    table.column(2).format().width(0).padding(0);
    table.column(3).format().width(CHOICE_COL_WIDTH).font_align(tabulate::FontAlign::center);
    table.column(4).format().width(TABLE_WIDTH - 2*CHOICE_COL_WIDTH - width - 4);
    for (int i = 1; i < table.size(); i++) {
        table[i][0].format().font_color(tabulate::Color::cyan);
        table[i][3].format().font_color(tabulate::Color::cyan);
    }
    std::cout << std::endl << table << std::endl;
    std::cout << "Which collectible types would you like to view? (e.g. \"2 3 5 8\"):" << std::endl;
    std::string line;
    std::getline(std::cin, line);
    std::regex re("\\d+");
    unsigned long long choice;
    for (std::sregex_iterator i = std::sregex_iterator(line.begin(), line.end(), re); i != std::sregex_iterator(); i++) {
        choice = std::stoull(i->str());
        if (choice < filterOptions.size()) {
            addFilterTypes(filterOptions[std::stoull(i->str())], allowedTypes, sortByType);
        }
    }
    // If no types specified, allow all types
    if (allowedTypes.empty()) {
        for (int i = 0; i < collectibleTypes.size(); i++) allowedTypes.insert(CollectibleEnum(i));
    }
    return sortByType;
}

// Returns whether the save is affected by the butterfly quest bug
// i.e. "Follow the Butterflies" is complete, but Butterfly Chest #1 is not collected
bool hasButterlyBug(std::vector<std::unordered_set<std::string>> &queryResults, std::unordered_set<TableEnum> &queryErrors) {
    if (queryErrors.contains(EconomicExpiryDynamic) || queryErrors.contains(PlayerStatsDynamic)) return false;
    // Check if the butterfly mission is completed
    if (!queryResults[PlayerStatsDynamic].contains("COM_11")) return false;
    // Get the quest's butterfly chest
    for ( const auto &collectible : collectibles ) {
        if (collectible.type == ButterflyChest && collectible.index == "1") {
            // If it hasn't been collected, then the bug happened
            return !queryResults[collectibleTypes[ButterflyChest].table].contains(collectible.key);
        }
    }
    return false; // Should never reach here
}

// Returns whether the save is affected by the missing conjuration bug
// i.e. the save has one less conjuration than conjuration chests collected
bool hasConjurationBug(std::vector<std::unordered_set<std::string>> &queryResults, std::unordered_set<TableEnum> &queryErrors, const unsigned long conjurationChestsOpened) {
    if (queryErrors.contains(CollectionDynamic2) || queryErrors.contains(LootDropComponentDynamic) || queryErrors.contains(EconomicExpiryDynamic) || queryErrors.contains(MapLocationDataDynamic)) return false;
    // Check if more chests than conjurations
    return (conjurationChestsOpened > queryResults[CollectionDynamic2].size());
}

// Runs Legilimens and returns whether it was successful
bool legilimize(const std::filesystem::path& saveFile, const std::filesystem::path &dbFile, const std::filesystem::path &outFile, const std::vector<std::string> &filters) {
    // Query all the necessary tables
    std::vector<std::unordered_set<std::string>> queryResults(tables.size());
    std::unordered_set<TableEnum> queryErrors;
    if (!readDB(saveFile, dbFile, queryResults, queryErrors)) return false;
    if (!queryErrors.empty()) {
        std::cerr << dye::red("SQLite was unable to read parts of the database") << std::endl;
        std::cerr << dye::red("The following collectible types were affected and won't work correctly:") << std::endl;
        bool first = true;
        for ( const auto &sqlTable : queryErrors ) {
            for ( const auto &collectibleType : tables[sqlTable].affected ) {
                if (!first) std::cerr << dye::red(", ");
                first = false;
                std::cerr << dye::red(collectibleType);
            }
        }
        std::cerr << std::endl;
    }
    std::unordered_set<CollectibleEnum> allowedTypes;
    bool sortByType = getFilters(filters, allowedTypes);
    // Get the missing collectibles in each region
    std::map<RegionEnum, std::vector<CollectibleStruct>> missingByRegion;
    std::map<CollectibleEnum, std::vector<CollectibleStruct>> missingByType;
    unsigned long conjurationChestsOpened = 0;
    CollectibleEnum cType;
    TableEnum cTable;
    for ( const auto &collectible : collectibles ) {
        cType = collectible.type;
        cTable = collectibleTypes[cType].table;
        if (queryErrors.contains(cTable)) continue;
        // If collected
        if (queryResults[cTable].contains(collectible.key)) {
            if (cType == MiscConjChest || cType == ArithmancyChest || cType == DungeonChest || cType == ButterflyChest || cType == VivariumChest) {
                conjurationChestsOpened += 1;
            }
            continue;
        }
        // If not included in filter
        if (!allowedTypes.contains(cType)) continue;
        // If not collected
        if (sortByType) {
            missingByType[cType].push_back(collectible);
        } else {
            missingByRegion[collectible.region].push_back(collectible);
        }
    }
    // Open output file
    std::ofstream fs = nullptr;
    if (!outFile.empty()) {
        fs = std::ofstream(outFile.string(), std::ios::out);
        if (fs.is_open()) {
            printTitle(fs);
            fs << std::endl << "Selected save file:" << std::endl << saveFile.string() << std::endl;
        }
    }
    if (missingByRegion.empty() && missingByType.empty()) {
        // Nothing was missing
        std::cout << std::endl << "Congratulations! You've gotten every collectible that Legilimens can detect." << std::endl;
        if (fs && fs.is_open()) {
            fs << std::endl << "Congratulations! You've gotten every collectible that Legilimens can detect." << std::endl;
        }
    } else if (sortByType) {
        // Sort by type
        tabulate::Table table, headerTable;
        for ( const auto &p : missingByType ) {
            headerTable = getTypeHeaderTable(p.first);
            table = getTypeTable();
            for (const auto &collectible : p.second) {
                addTypeTableRow(table, collectible);
            }
            table.column(1).format().font_align(tabulate::FontAlign::center);
            std::cout << std::endl << std::endl << headerTable << std::endl << table << std::endl;
            if (fs && fs.is_open()) {
                fs << std::endl << std::endl << headerTable << std::endl << table << std::endl;
            }
        }
    } else {
        // Sort by region
        tabulate::Table table, headerTable;
        for ( const auto &p : missingByRegion ) {
            headerTable = getRegionHeaderTable(p.first);
            table = getRegionTable();
            for (const auto &collectible : p.second) {
                addRegionTableRow(table, collectible);
            }
            table.column(1).format().font_align(tabulate::FontAlign::center);
            std::cout << std::endl << std::endl << headerTable << std::endl << table << std::endl;
            if (fs && fs.is_open()) {
                fs << std::endl << std::endl << headerTable << std::endl << table << std::endl;
            }
        }
    }
    // Check for bugs
    if (hasButterlyBug(queryResults, queryErrors)) {
        std::cout << std::endl << dye::red("Your save seems to be affected by the butterfly quest bug. If you're unable to collect\nButterfly Chest #1, consider using https://hogwarts-legacy-save-editor.vercel.app to fix it.") << std::endl;
        if (fs && fs.is_open()) {
            fs << std::endl << dye::red("Your save seems to be affected by the butterfly quest bug. If you're unable to collect\nButterfly Chest #1, consider using https://hogwarts-legacy-save-editor.vercel.app to fix it.") << std::endl;
        }
    }
    if (hasConjurationBug(queryResults, queryErrors, conjurationChestsOpened)) {
        std::cout << std::endl << dye::red("Your save seems to be affected by the 139/140 conjuration bug. If you can't find your last\nexploration conjuration, consider using https://www.nexusmods.com/hogwartslegacy/mods/832 to fix it.") << std::endl;
        if (fs && fs.is_open()) {
            fs << std::endl << dye::red("Your save seems to be affected by the 139/140 conjuration bug. If you can't find your last\nexploration conjuration, consider using https://www.nexusmods.com/hogwartslegacy/mods/832 to fix it.") << std::endl;
        }
    }
    if (fs && fs.is_open()) {
        fs.close();
    }
    return true;
}

// Returns the absolute path to the output text file, empty if no output
std::filesystem::path getOutputFile(const std::filesystem::path &exePath, const argparse::ArgumentParser &parsedArgs) {
    auto filename = parsedArgs.get<std::string>("-o");
    // If user chooses to not output to any file
    if (parsedArgs.is_used("-o") && filename == DEFAULT_OUTPUT_FILE) {
        std::filesystem::path result;
        return result;
    }
    // Fill in timestamp (only first occurrence)
    size_t index = filename.find("{TIMESTAMP}");
    if (index != std::string::npos) {
        std::chrono::time_point currentTime = std::chrono::system_clock::now();
        auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(currentTime.time_since_epoch());
        filename.replace(index, 11, std::to_string(timeSinceEpoch.count()));
    }
    // Convert to absolute path
    std::filesystem::path result(filename);
    if (result.is_relative()) result = exePath.parent_path() / result;
    return result;
}

// Runs the program, except the final "Press enter to close", and returns whether it succeeds
bool run(const std::filesystem::path &exePath, const argparse::ArgumentParser &parsedArgs) {
    printTitle(std::cout);
    // Get save path
    std::filesystem::path saveFile(parsedArgs.get<std::string>("file"));
    if (saveFile.empty()) saveFile = getSavePath();
    // Get temp DB file
    std::filesystem::path dbFile;
    if (!getTempDBFile(exePath, dbFile)) return false;
    // Get output file
    std::filesystem::path outFile = getOutputFile(exePath, parsedArgs);
    // Run
    return legilimize(saveFile, dbFile, outFile, parsedArgs.get<std::vector<std::string>>("--filters"));
}

int main(int argc, char *argv[]) {
    bool success;
    argparse::ArgumentParser parsedArgs = parseArgs(argc, argv, success);
    if (!success) return 1;
    success = run(std::filesystem::path(argv[0]), parsedArgs);
    if (!parsedArgs.get<bool>("--dont-confirm-exit")) {
        std::cout << std::endl << "Press enter to close this window...";
        getchar();
    }
    return !success;
}
