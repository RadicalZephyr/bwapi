// See shadow_imports.cpp: the imports BWAPI detours, which this host has to carry for
// HackUtil::PatchImport to have anything to find.
#pragma once

// The twenty-four functions have twenty-four signatures and are only ever stored and compared, so
// they are held as one generic function pointer type. A function pointer does not convert to void*
// in standard C++ - MSVC allows it as an extension and clang does not - and reinterpret_cast
// between function pointer types is the portable spelling.
typedef void (*AnyProc)();

struct DetouredImport
{
  const char *module;   // as the import descriptor names it, compared case-insensitively
  const char *symbol;   // "#119" for an ordinal, the exported name otherwise
  AnyProc     address;  // referenced so the linker keeps the import
};

extern const DetouredImport detouredImports[];
extern const unsigned       detouredImportCount;

void referenceDetouredImports();
