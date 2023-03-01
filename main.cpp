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
#define VERSION "0.2.2"

void printTitle() {
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
    table.add_row({"https://github.com/Malin001/Legilimens-Hogwarts-cpp"});
    std::string versionStr = "Version ";
    versionStr += VERSION;
    versionStr += " - Created by Malin";
    table.add_row({versionStr});
    table.row(0).format().show_border_top();
    for (int i = 0; i < 8; i++) table.row(i).format().padding_left(0).padding_right(0);
    table.row(8).format().show_border_top();
    std::cout << table << std::endl;
}

argparse::ArgumentParser parseArgs(int argc, char *argv[], bool &success) {
    argparse::ArgumentParser program("Legilimens", VERSION);
    program.add_argument("file").default_value(std::string{""}).help("Path of your .sav Hogwarts Legacy save file. Will be prompted if empty");
    program.add_argument("--dont-confirm-exit").default_value(false).implicit_value(true).help("Gets rid of the \"Press enter to close this window...\" prompt");
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
        if (sqlite3_prepare_v2(db, queries[index].c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            queryErrors.insert(TableEnum(index));
        } else if (TableEnum(index) != AchievementDynamic) {
            while (sqlite3_step(stmt) != SQLITE_DONE) {
                queryResults[index].insert(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
            }
        } else if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string commaSepList = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::regex re("\\w+");
            for (std::sregex_iterator i = std::sregex_iterator(commaSepList.begin(), commaSepList.end(), re); i != std::sregex_iterator(); i++) {
                queryResults[index].insert(i->str());
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
        for (int i = 0; i < queries.size(); i++) {
            runQuery(db, stmt, i, queryResults, queryErrors);
        }
    } else {
        std::cerr << dye::red("SQLite was unable to read the database") << std::endl;
    }
    if (queryErrors.size() == queries.size()) {
        std::cerr << dye::red("SQLite was unable to read the database") << std::endl;
        err = SQLITE_ERROR;
    } else {
        err = SQLITE_OK;
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

// Adds a row to the table for the given collectible
void addCollectibleRow(tabulate::Table &table, const CollectibleStruct& collectible) {
    std::string name = (collectible.type == FinishingTouchEnemy) ? collectible.index : timestampNames[collectible.type] + " #" + collectible.index;
    table.add_row({name, timestampParen[collectible.type], "https://youtu.be/" + videoIds[collectible.video] + "&t=" + std::to_string(collectible.timestamp)});
    tabulate::Color color = tabulate::Color::white;
    if (timestampNames[collectible.type] == "Field guide page") {
        color = tabulate::Color::cyan;
    } else if (timestampNames[collectible.type] == "Demiguise Moon") {
        color = tabulate::Color::blue;
    } else if (timestampNames[collectible.type] == "Merlin Trial") {
        color = tabulate::Color::green;
    } else if (timestampNames[collectible.type] == "Butterfly Chest") {
        color = tabulate::Color::yellow;
    } else if (timestampNames[collectible.type] == "Collection Chest") {
        color = tabulate::Color::magenta;
    }
    table[table.size()-1].format().font_color(color);
}

// Gets a header table for the given region
tabulate::Table getRegionHeaderTable(RegionEnum region) {
    tabulate::Table table;
    if (globalRegionNames[region].empty()) {
        table.add_row({regionNames[region]});
    } else {
        table.add_row({globalRegionNames[region] + " - " + regionNames[region]});
    }
    table[0].format().font_align(tabulate::FontAlign::center).hide_border().width(TABLE_WIDTH).padding_bottom(0);
    return table;
}

// Gets a table for the given region, returns the name of the region
tabulate::Table getRegionTable() {
    tabulate::Table table;
    table.add_row({"Item", "Type", "Location"});
    table.column(0).format().width(27);
    table.column(1).format().width(TABLE_WIDTH - 64 - 2);
    table.column(2).format().width(37);
    return table;
}

// Gets the list of filters to use
void getFilters(const std::vector<std::string> &filters, std::unordered_set<CollectibleEnum> &allowedTypes) {
    // Check command line args
    for ( const auto &filter : filters ) {
        for ( const auto &option : filterOptions ) {
            if (filter == option.cli) {
                for ( const auto &collectibleType : option.types ) allowedTypes.insert(collectibleType);
                break;
            }
        }
    }
    if (!filters.empty()) return;
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
        for ( const auto &collectibleType : filterOptions[choice].types ) allowedTypes.insert(collectibleType);
    }
}

// Runs Legilimens and returns whether it was successful
bool legilimize(const std::filesystem::path& saveFile, const std::filesystem::path &dbFile, const std::vector<std::string> &filters) {
    // Query all the necessary tables
    std::vector<std::unordered_set<std::string>> queryResults(queries.size());
    std::unordered_set<TableEnum> queryErrors;
    if (!readDB(saveFile, dbFile, queryResults, queryErrors)) return false;
    if (!queryErrors.empty()) {
        std::cerr << dye::red("SQLite was unable to read parts of the database") << std::endl;
        std::cerr << dye::red("The following collectible types were affected and won't work correctly:") << std::endl;
        bool first = true;
        for ( const auto &sqlTable : queryErrors ) {
            for ( const auto &collectibleType : affectedCollectibles[sqlTable] ) {
                if (!first) std::cerr << dye::red(", ");
                first = false;
                std::cerr << dye::red(collectibleType);
            }
        }
        std::cerr << std::endl;
    }
    std::unordered_set<CollectibleEnum> allowedTypes;
    getFilters(filters, allowedTypes);
    // Get the missing collectibles in each region
    std::map<RegionEnum, std::vector<CollectibleStruct>> missing;
    for ( const auto &collectible : collectibles) {
        if (!queryErrors.contains(collectibleTables[collectible.type]) && (!allowedTypes.contains(collectible.type) || queryResults[collectibleTables[collectible.type]].contains(collectible.key))) continue;
        missing[collectible.region].push_back(collectible);
    }
    // If nothing was missing
    if (missing.empty()) {
        std::cout << std::endl << "Congratulations! You've gotten every collectible that Legilimens can detect." << std::endl;
        return true;
    }
    // Create a table for each region and print it
    tabulate::Table table;
    for ( const auto &p : missing ) {
        std::cout << std::endl << std::endl << getRegionHeaderTable(p.first) << std::endl;
        table = getRegionTable();
        for (const auto &collectible : p.second) {
            addCollectibleRow(table, collectible);
        }
        table.column(1).format().font_align(tabulate::FontAlign::center);
        std::cout << table << std::endl;
    }
    return true;
}

// Runs the program, except the final "Press enter to close", and returns whether it succeeds
bool run(const std::filesystem::path &exePath, const argparse::ArgumentParser &parsedArgs) {
    printTitle();
    // Get save path
    std::filesystem::path saveFile(parsedArgs.get<std::string>("file"));
    if (saveFile.empty()) saveFile = getSavePath();
    // Get temp DB file
    std::filesystem::path dbFile;
    if (!getTempDBFile(exePath, dbFile)) return false;
    // Run
    return legilimize(saveFile, dbFile, parsedArgs.get<std::vector<std::string>>("--filters"));
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
