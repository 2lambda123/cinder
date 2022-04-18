#include "Jit/codegen/code_section.h"

#include "Jit/log.h"

namespace jit::codegen {
const char* codeSectionName(CodeSection section) {
  switch (section) {
    case CodeSection::kHot:
      return ".text";
    case CodeSection::kCold:
      return ".coldtext";
  }
  JIT_CHECK(false, "Bad code section %d", static_cast<int>(section));
}

CodeSection codeSectionFromName(const char* name) {
  if (strcmp(name, ".text") == 0 || strcmp(name, ".addrtab") == 0) {
    return CodeSection::kHot;
  }
  if (strcmp(name, ".coldtext") == 0) {
    return CodeSection::kCold;
  }
  JIT_CHECK(false, "Bad code section name %s", name);
}

void populateCodeSections(
    std::vector<std::pair<void*, std::size_t>>& code_sections,
    asmjit::CodeHolder& code,
    void* entry) {
  forEachSection([&](CodeSection section) {
    auto asmjit_section = code.sectionByName(codeSectionName(section));
    if (asmjit_section == nullptr || asmjit_section->realSize() == 0) {
      return;
    }
    auto section_start = static_cast<char*>(entry) + asmjit_section->offset();
    code_sections.push_back(std::make_pair(
        reinterpret_cast<void*>(section_start), asmjit_section->realSize()));
  });
}

} // namespace jit::codegen
