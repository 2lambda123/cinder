// Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)

#pragma once

#include "Python.h"

#include "Jit/runtime.h"

#include <cstdint>
#include <istream>
#include <ostream>
#include <regex>

namespace jit {

// Pattern to strip from filenames while computing CodeKeys.
extern std::regex profileDataStripPattern;

// Load serialized profile data from the given filename or stream, returning
// true on success.
//
// Binary format is defined in Jit/profile_data_format.txt
bool readProfileData(const std::string& filename);
bool readProfileData(std::istream& stream);

// Write profile data from the current process to the given filename or stream,
// returning true on success.
bool writeProfileData(const std::string& filename);
bool writeProfileData(std::ostream& stream);

// Clear any loaded profile data.
void clearProfileData();

// Store a list of profiles of type names for all operands of an instruction
using PolymorphicProfiles = std::vector<std::vector<std::string>>;

// Store a list of profiles of types for all operands of an instruction
using PolymorphicTypes = std::vector<std::vector<BorrowedRef<PyTypeObject>>>;

// Map from bytecode offset within a code object to vector of vector of string
// type names for each operand of an instruction
using CodeProfileData = UnorderedMap<BCOffset, PolymorphicProfiles>;

// Look up the profile data for the given code object, returning nullptr if
// there is none.
const CodeProfileData* getProfileData(PyCodeObject* code);

// Return a list types materialized from a CodeProfileData and a BCOffset. The
// result will be empty if there's no data for bc_off.
PolymorphicTypes getProfiledTypes(const CodeProfileData& data, BCOffset bc_off);

// Check whether the given type has split dict keys primed from profile data,
// which implies that they are unlikely to change at runtime.
bool hasPrimedDictKeys(BorrowedRef<PyTypeObject> type);

// Return the number of cached split dict keys in the given type.
int numCachedKeys(BorrowedRef<PyTypeObject> type);

// Call `callback' 0 or more times, once for each split dict key in the given
// type.
void enumerateCachedKeys(
    BorrowedRef<PyTypeObject> type,
    std::function<void(BorrowedRef<>)> callback);

// Inform the profiling code that a type has been created.
void registerProfiledType(PyTypeObject* type);

// Inform the profiling code that a type is about to be destroyed.
void unregisterProfiledType(PyTypeObject* type);

} // namespace jit
