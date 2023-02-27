#ifndef LEGILIMENS_HOGWARTS_LEGACY_CPP_GETSAVE_H
#define LEGILIMENS_HOGWARTS_LEGACY_CPP_GETSAVE_H

#include <filesystem>

#define MAGIC_HEADER "GVAS"
#define TABLE_WIDTH 85
#define CHOICE_COL_WIDTH 9

unsigned int readU32(const std::string &bytes, unsigned long long index);
std::filesystem::path getSavePath();

#endif //LEGILIMENS_HOGWARTS_LEGACY_CPP_GETSAVE_H
