#pragma once
/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

inline std::string loadFile(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) {
        throw std::runtime_error("Cannot open: " + p.string());
    }
    return {std::istreambuf_iterator<char>(f), {}};
}
