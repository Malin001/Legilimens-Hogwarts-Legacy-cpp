#include "getsave.h"
#include <vector>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <shlobj.h>
#include <map>
#include <chrono>
#include "tabulate.hpp"

#define CHAR_NAME_STR "CharacterName\x00"
#define CHAR_HOUSE_STR "CharacterHouse\x00"
#define CHAR_NAME_OFFSET 43
#define CHAR_HOUSE_OFFSET 44

unsigned int readU32(const std::string &bytes, unsigned long long index) {
    return (unsigned char)(bytes[index+3]) << 24 | (unsigned char)(bytes[index+2]) << 16 | (unsigned char)(bytes[index+1]) << 8 | (unsigned char)(bytes[index]);
}

// Gets the character name and house from a save file, returns whether it's a valid save file
bool readSaveInfo(const std::filesystem::path& savePath, std::string &charName, std::string &charHouse) {
    // Try to open the file
    if (!std::filesystem::exists(savePath)) return false;
    std::ifstream fs(savePath, std::ios::in|std::ios::binary);
    if (!fs.is_open()) return false;
    // Read the file
    std::ostringstream sstr;
    sstr << fs.rdbuf();
    std::string saveData = sstr.str();
    fs.close();
    // Check magic header
    if (!saveData.starts_with(MAGIC_HEADER)) return false;
    // Find character name
    std::size_t found = saveData.find(CHAR_NAME_STR);
    unsigned int strLength;
    if (found != std::string::npos && found + CHAR_NAME_OFFSET < saveData.length()) {
        strLength = readU32(saveData, found + CHAR_NAME_OFFSET - 4) - 1;
        if (strLength > 0 && found + CHAR_NAME_OFFSET + strLength <= saveData.length()) {
            charName = saveData.substr(found + CHAR_NAME_OFFSET, strLength);
        }
    }
    // Find character house
    found = saveData.find(CHAR_HOUSE_STR);
    if (found != std::string::npos && found + CHAR_HOUSE_OFFSET < saveData.length()) {
        strLength = readU32(saveData, found + CHAR_HOUSE_OFFSET - 4) - 1;
        if (strLength > 0 && found + CHAR_HOUSE_OFFSET + strLength <= saveData.length()) {
            charHouse = saveData.substr(found + CHAR_HOUSE_OFFSET, strLength);
        }
    }
    return true;
}

// Returns whether file is a valid save
bool isValid(const std::filesystem::path& savePath) {
    // Make sure format is "HL-xx-xx.sav" and that file existss
    if (savePath.extension().string() != ".sav") return false;
    if (savePath.filename().string().length() != 12) return false;
    if (!savePath.filename().string().starts_with("HL-")) return false;
    if (!std::filesystem::exists(savePath)) return false;
    // Check for magic header b"GVAS"
    std::ifstream fs(savePath, std::ios::in|std::ios::binary);
    if (!fs.is_open()) return false;
    std::string header;
    char* buffer = new char[5];
    for (int i = 0; i < 5; i++) buffer[i] = '\x00';
    fs.read(buffer, 4);
    fs.close();
    header = buffer;
    return (header == MAGIC_HEADER);
}

struct SaveList {
    std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> paths;
    std::string charName;
    std::string charHouse;
};

bool pairCompare(const std::pair<std::filesystem::path, std::filesystem::file_time_type> &a,
                 const std::pair<std::filesystem::path, std::filesystem::file_time_type> &b) {
    return (a.second == b.second) ? (a.first < b.first) : (a.second > b.second);
}

// Gets lists of potential save files, where each list corresponds to a single character
std::vector<SaveList> getSaveList() {
    std::vector<SaveList> result;
    // Find %LocalAppData%
    std::filesystem::path localAppData, users;
    PWSTR path_tmp;
    auto err = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_tmp);
    if (err != S_OK) {
        CoTaskMemFree(path_tmp);
        return result;
    }
    localAppData = path_tmp;
    CoTaskMemFree(path_tmp);
    // Find users folder
    users = localAppData / "HogwartsLegacy" / "Saved" / "SaveGames";
    if (!std::filesystem::is_directory(users)) users = localAppData / "Hogwarts Legacy" / "Saved" / "SaveGames";
    if (!std::filesystem::is_directory(users)) return result;
    // Find each user
    std::string charIndex;
    for (auto const& userFolder : std::filesystem::directory_iterator{users}) {
        if (!std::filesystem::is_directory(userFolder)) continue;
        std::unordered_map<std::string, std::vector<std::filesystem::path>> savesByChar;
        // Get saves for each user,
        for (auto const& saveFile : std::filesystem::directory_iterator{userFolder}) {
            if (!isValid(saveFile.path())) continue;
            charIndex = saveFile.path().filename().string().substr(3, 2);  // "HL-01-11.sav" -> "01"
            savesByChar[charIndex].push_back(saveFile.path());
        }
        // Add to save list
        for ( auto &p : savesByChar ) {
            SaveList saves = {{}, "", ""};
            for ( const auto &savePath : p.second ) {
                if (saves.charName.empty() || saves.charHouse.empty()) {
                    readSaveInfo(savePath, saves.charName, saves.charHouse);
                }
                saves.paths.emplace_back(savePath, std::filesystem::last_write_time(savePath));
            }
            sort(saves.paths.begin(), saves.paths.end(), pairCompare);
            if (saves.charName.empty()) saves.charName = "Unknown name";
            if (saves.charHouse.empty()) saves.charHouse = "Unknown house";
            if (!saves.paths.empty()) result.push_back(saves);
        }
    }
    return result;
}

// e.g. "Manual #1", "Autosave #3", etc.
std::string getSaveType(const std::filesystem::path& savePath) {
    std::string filename = savePath.filename().string();
    std::string result = (filename.substr(6, 1) == "0") ? "Manual Save #" : "Autosave #";
    return result + std::to_string(filename[7] - '0' + 1);
}

// Converts file_time_type to string
std::string timeToString(std::filesystem::file_time_type time) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::stringstream buffer;
    buffer << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M");
    return buffer.str();
}

// Gets input from the user, forced within the given range
// https://stackoverflow.com/questions/41475922/prompt-user-input-until-correct-c
unsigned int getChoice(unsigned int maxVal, const std::string &prompt) {
    unsigned int choice;
    std::string line;
    while (true) {
        std::cout << prompt << std::endl;
        std::getline(std::cin, line);
        std::istringstream iss(line);
        if (iss >> choice >> std::ws && iss.get() == EOF && choice <= maxVal) {
            return choice;
        }
        std::cout << "Invalid choice, must be between 0 and " << maxVal << ", inclusive." << std::endl;
    }
}

// Prompts the user to select a save path for a single character. Returns true on success, false on back
bool getSavePathForChar(SaveList saves, std::filesystem::path &result) {
    tabulate::Table table;
    table.add_row({"Choice", "Save File", "Type", "Time"});
    for (int i = 0; i < saves.paths.size(); i++) {
        table.add_row({std::to_string(i), saves.paths[i].first.filename().string(), getSaveType(saves.paths[i].first), timeToString(saves.paths[i].second)});
    }
    table.add_row({std::to_string(saves.paths.size()), "Go Back", "", ""});
    for ( auto &cell : table.column(2) ) {
        if (cell.get_text().starts_with("A")) {
            cell.format().font_color(tabulate::Color::yellow);
        } else if (cell.get_text().starts_with("M")) {
            cell.format().font_color(tabulate::Color::green);
        }
    }
    table.column(0).format().width(CHOICE_COL_WIDTH).font_align(tabulate::FontAlign::center);
    table.column(1).format().width(20).font_align(tabulate::FontAlign::center);
    table.column(2).format().width(20).font_align(tabulate::FontAlign::center);
    table.column(3).format().width(TABLE_WIDTH - CHOICE_COL_WIDTH - 40 - 3).font_align(tabulate::FontAlign::center);
    for (int i = 1; i < table.size(); i++) table[i][0].format().font_color(tabulate::Color::cyan);
    std::cout << std::endl << table << std::endl;
    unsigned int choice = getChoice(saves.paths.size(), "Which save file for " + saves.charName + " (" + saves.charHouse + ") should Legilimens use?");
    if (choice < saves.paths.size()) {
        result = saves.paths[choice].first;
        return true;
    }
    return false;
}

// Prompts the user to select a character, then a save for that character. Returns true on success, false on back
bool getCharacter(std::vector<SaveList> &saves, std::filesystem::path &result) {
    tabulate::Table table;
    table.add_row({"Choice", "Name", "House"});
    for (int i = 0; i < saves.size(); i++) {
        table.add_row({std::to_string(i), saves[i].charName, saves[i].charHouse});
    }
    table.add_row({std::to_string(saves.size()), "Go back", ""});
    for ( auto &cell : table.column(2) ) {
        if (cell.get_text().starts_with("G")) {
            cell.format().font_color(tabulate::Color::red);
        } else if (cell.get_text().starts_with("S")) {
            cell.format().font_color(tabulate::Color::green);
        } else if (cell.get_text().starts_with("Hu")) {
            cell.format().font_color(tabulate::Color::yellow);
        } else if (cell.get_text().starts_with("R")) {
            cell.format().font_color(tabulate::Color::blue);
        }
    }
    table.column(0).format().width(CHOICE_COL_WIDTH).font_align(tabulate::FontAlign::center);
    table.column(1).format().width(TABLE_WIDTH - CHOICE_COL_WIDTH - 15 - 2);
    table.column(2).format().width(15).font_align(tabulate::FontAlign::center);
    for (int i = 1; i < table.size(); i++) table[i][0].format().font_color(tabulate::Color::cyan);
    unsigned int choice;
    while (true) {
        std::cout << std::endl << table << std::endl;
        choice = getChoice(saves.size(), "Which character should Legilimens use?");
        if (choice >= saves.size()) return false;
        if (getSavePathForChar(saves[choice], result)) return true;
    }
}

std::filesystem::path manuallyInputPath() {
    std::string saveFile;
    std::cout << "Path to .sav file:" << std::endl;
    std::getline(std::cin, saveFile);
    // Remove start/end quotes if necessary
    if (saveFile.length() > 1 && saveFile[0] == '"' && saveFile.back() == '"') {
        saveFile = saveFile.substr(1, saveFile.length() - 2);
    }
    std::filesystem::path result(saveFile);
    return result;
}

// Prompts user for manual input or auto save detection
std::filesystem::path getSavePath() {
    tabulate::Table table;
    table.add_row({"Choice", "Option"});
    table.add_row({"0", "Automatically detect saves"});
    table.add_row({"1", "Manual input"});
    table.column(0).format().width(CHOICE_COL_WIDTH).font_align(tabulate::FontAlign::center);
    table.column(1).format().width(TABLE_WIDTH - CHOICE_COL_WIDTH - 1);
    for (int i = 1; i < table.size(); i++) table[i][0].format().font_color(tabulate::Color::cyan);
    unsigned int choice;
    std::vector<SaveList> saves;
    bool scanned = false;
    std::filesystem::path result;
    while (true) {
        std::cout << std::endl << table << std::endl;
        choice = getChoice(1, "How would you like to find your save path?");
        if (choice == 1) return manuallyInputPath();
        if (!scanned) {
            saves = getSaveList();
            scanned = true;
        }
        if (saves.empty()) {
            std::cout << "Legilimens was unable to detect any save files." << std::endl;
            return manuallyInputPath();
        }
        if (getCharacter(saves, result)) return result;
    }
}
