#pragma once

#include "../GUIContext.h"
#include "core/decompiler/DecompilerContext.h"

namespace ShadPKG::GUI {

class StructEditorView {
public:
  StructEditorView();
  ~StructEditorView() = default;

  void Draw(GUIContext &ctx);

private:
  // Local state for the editor
  std::string newStructName_;
  int selectedStructIndex_ = -1;
};

} // namespace ShadPKG::GUI
