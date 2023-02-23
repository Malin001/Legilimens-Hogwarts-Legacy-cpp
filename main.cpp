#include <iostream>
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <map>
#include "sqlite3.h"
#include "collectibles.h"

#define MAGIC_HEADER "GVAS"
#define DB_IMAGE_STR "RawDatabaseImage"

void printTitle() {
    std::cout << R"(  _                _ _ _                          )" << std::endl;
    std::cout << R"( | |              (_) (_)                         )" << std::endl;
    std::cout << R"( | |     ___  __ _ _| |_ _ __ ___   ___ _ __  ___ )" << std::endl;
    std::cout << R"( | |    / _ \/ _` | | | | '_ ` _ \ / _ \ '_ \/ __|)" << std::endl;
    std::cout << R"( | |___|  __/ (_| | | | | | | | | |  __/ | | \__ \)" << std::endl;
    std::cout << R"( |______\___|\__, |_|_|_|_| |_| |_|\___|_| |_|___/)" << std::endl;
    std::cout << R"(              __/ |                               )" << std::endl;
    std::cout << R"(             |___/                                )" << std::endl << std::endl;
    std::cout << "A Hogwarts Legacy tool to find your missing collectibles, created by Malin" << std::endl;
    std::cout << "If you encounter any errors or unexpected results, you can create an issue on GitHub with your save file attached:" << std::endl;
    std::cout << "https://github.com/Malin001/Legilimens-Hogwarts-cpp/issues" << std::endl << std::endl;
}

// Returns the path to the save file, which is either gotten from the command line or by prompting the user
std::string getSaveFile(int argc, char *argv[]) {
    if (argc > 1) {
        std::string saveFile(argv[1]);
        return saveFile;
    }
    std::string saveFile;
    std::cout << "Path to .sav file:" << std::endl;
    std::getline(std::cin, saveFile);
    std::cout << std::endl;
    return saveFile;
}

// Gets the first available file path temp_X.db to temporarily store the database, return whether it was successful
bool getTempDBFile(char *argv[], std::filesystem::path &dbFile) {
    std::filesystem::path exePath(argv[0]);
    for (int i = 0; i < 1000; i++) {
        dbFile = exePath.parent_path() / ("temp_" + std::to_string(i) + ".db");
        if (!std::filesystem::exists(dbFile)) return true;
    }
    std::cout << "Legilimens was unable to create a temporary database file" << std::endl;
    return false;
}

// Sets dbData to the content of the database contained in the saveFile, and returns whether it was successful
bool extractDB(std::string &saveFile, std::string &dbData) {
    // Remove start/end quotes if necessary
    if (saveFile.length() > 1 && saveFile[0] == '"' && saveFile.back() == '"') {
        saveFile = saveFile.substr(1, saveFile.length() - 2);
    }
    // Check file existence
    std::filesystem::path f(saveFile);
    if (!std::filesystem::exists(f)) {
        std::cout << "Legilimens was not able to find the file \"" << saveFile << "\"" << std::endl;
        return false;
    }
    // Check for other errors
    std::ifstream fs(saveFile, std::ios::in|std::ios::binary);
    if (!fs.is_open()) {
        std::cout << "Legilimens encountered an error reading the file \"" << saveFile << "\"" << std::endl;
        return false;
    }
    // Read the file
    std::ostringstream sstr;
    sstr << fs.rdbuf();
    std::string saveData = sstr.str();
    fs.close();
    // Check magic header
    if (!saveData.starts_with(MAGIC_HEADER)) {
        std::cout << "File \"" << saveFile << "\" doesn't seem to be a Hogwarts Legacy save file" << std::endl;
        return false;
    }
    // Find DB start and size
    std::size_t found = saveData.find(DB_IMAGE_STR);
    if (found == std::string::npos || found + 65 >= saveData.length()) {
        std::cout << "Legilimens was unable to find the SQL database in your save file" << std::endl;
        return false;
    }
    unsigned long long dbStartIndex = found + 65;
    unsigned int dbSize = (unsigned char)(saveData[dbStartIndex-1]) << 24 | (unsigned char)(saveData[dbStartIndex-2]) << 16 | (unsigned char)(saveData[dbStartIndex-3]) << 8 | (unsigned char)(saveData[dbStartIndex-4]);
    // Extract DB
    dbData = saveData.substr(dbStartIndex, dbSize);
    return true;
}

// Read the tables in the database, and returns whether it was successful
bool readDB(std::string &saveFile, const std::filesystem::path &dbFile, std::unordered_set<std::string>* queryResults) {
    // Create a new file containing the database
    std::string dbData;
    if (!extractDB(saveFile, dbData)) return false;
    std::ofstream fs(dbFile.string(), std::ios::out|std::ios::binary);
    if (!fs.is_open()) {
        std::cout << "Legilimens was unable to write the database to a new file" << std::endl;
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
            err = sqlite3_prepare_v2(db, queries[i].c_str(), -1, &stmt, nullptr);
            if (err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                break;
            }
            while (sqlite3_step(stmt) != SQLITE_DONE) {
                queryResults[i].insert(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
            }
            sqlite3_finalize(stmt);
        }
    }
    if (err != SQLITE_OK) {
        std::cout << "SQLite error:" << std::endl;
        std::cout << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);
    // Remove database file
    std::error_code ec;
    if (std::filesystem::exists(dbFile) && !std::filesystem::remove(dbFile, ec)) {
        std::cout << "Error removing database file \"" << dbFile << "\"" << std::endl;
    }
    return (err == SQLITE_OK);
}

// Converts a collectible struct into a printable string
std::string getCollectibleString(const CollectibleStruct& collectible) {
    std::string result = timestampNames[collectible.type] + " #" + std::to_string(collectible.index);
    if (!timestampParen[collectible.type].empty()) {
        result += " (" + timestampParen[collectible.type] + ")";
    }
    return result + " - https://youtu.be/" + videoIds[collectible.video] + "&=" + std::to_string(collectible.timestamp);
}

// Runs Legilimens and returns whether it was successful
bool legilimize(std::string& saveFile, const std::filesystem::path &dbFile) {
    // Query all the necessary tables
    std::unordered_set<std::string> queryResults[queries.size()];
    if (!readDB(saveFile, dbFile, queryResults)) return false;
    // Get the missing collectibles in each region
    std::map<RegionEnum, std::vector<std::string>> missing;
    for (const auto &collectible : collectibles) {
        if (!queryResults[collectibleTables[collectible.type]].contains(collectible.key)) {
            missing[collectible.region].push_back(getCollectibleString(collectible));
        }
    }
    // If nothing was missing
    if (missing.empty()) {
        std::cout << "Congratulations! You've gotten every collectible that Legilimens can detect." << std::endl;
        return true;
    }
    // Print output for each region
    bool first = true;
    for ( const auto &p : missing ) {
        if (!first) std::cout << std::endl;
        first = false;
        if (!globalRegionNames[p.first].empty()) {
            std::cout << globalRegionNames[p.first] << " - ";
        }
        std::cout << regionNames[p.first] << std::endl;
        for (const auto &line : p.second ) {
            std::cout << "\t" << line << std::endl;
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    printTitle();
    std::string saveFile = getSaveFile(argc, argv);
    std::filesystem::path dbFile;
    if (getTempDBFile(argv, dbFile)) {
        legilimize(saveFile, dbFile);
    }
    std::cout << std::endl << "Press enter to close this window...";
    getchar();
    return 0;
}
