#pragma once

#include "../GUIContext.h"
#include "core/decompiler/DecompilerContext.h"
#include "core/file_format/pkg.h"
#include <memory>

namespace ShadPKG::GUI {

class DecompilerView {
public:
  DecompilerView();
  ~DecompilerView() = default;

  void Draw(GUIContext &ctx);
  void LoadBinary(const std::string &path);

private:
  std::unique_ptr<ShadPKG::Decompiler::DecompilerContext> decompilerCtx_;

  // UI State
  std::string retypeSearchBuffer_;
  bool isRetyping_ = false;
  std::string variableToRetype_;

  void DrawRetypePopup();

  ShadPKG::Decompiler::IR::Function *selectedFunction_ = nullptr;
  std::shared_ptr<PKG> lastPkg_; // Track current PKG to detect changes
  bool showDisassembly_ = true;
  bool showPseudocode_ = true;

  // Text Editor state (using ImGui input text for now, or just text rendering)
  std::string currentCode_;
  bool codeDirty_ = true; // Use to trigger regeneration

  // Interactive State
  uint64_t renameAddress_ = 0;
  char renameBuffer_[256] = "";
};

} // namespace ShadPKG::GUI
