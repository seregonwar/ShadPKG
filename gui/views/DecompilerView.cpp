#include "gui/include/views/DecompilerView.h"
#include "common/io_file.h"
#include "gui/include/StyleManager.h"
#include "imgui.h"
#include <sstream>

namespace ShadPKG::GUI {

DecompilerView::DecompilerView() {
  decompilerCtx_ = std::make_unique<ShadPKG::Decompiler::DecompilerContext>();
  decompilerCtx_ = std::make_unique<ShadPKG::Decompiler::DecompilerContext>();
  retypeSearchBuffer_.resize(256);
}

void DecompilerView::LoadBinary(const std::string &path) {
  // Unused
}

void DecompilerView::Draw(GUIContext &ctx) {
  // Zero-Friction: Detect loaded PKG
  if (ctx.currentPkg != lastPkg_) {
    lastPkg_ = ctx.currentPkg;
    selectedFunction_ = nullptr;

    if (lastPkg_) {
      // Find executable
      std::vector<u8> buffer = lastPkg_->GetFileBuffer("eboot.bin");
      if (buffer.empty()) {
        // Try searching
        auto files = lastPkg_->GetFileList();
        for (const auto &f : files) {
          if (f.find("eboot.bin") != std::string::npos ||
              f.find(".elf") != std::string::npos) {
            buffer = lastPkg_->GetFileBuffer(f);
            break;
          }
        }
      }

      if (!buffer.empty()) {
        if (decompilerCtx_->LoadELF(buffer)) {
          decompilerCtx_->Analyze();
        } else {
          decompilerCtx_->LoadBinary(buffer, 0x400000);
          decompilerCtx_->Analyze();
        }

        if (!decompilerCtx_->GetFunctions().empty()) {
          selectedFunction_ = decompilerCtx_->GetFunctions()[0].get();
        }
      }
    }
  }

  ImGui::BeginGroup();

  // Left Panel: Function List
  ImGui::BeginChild("FunctionList", ImVec2(250, 0), true);
  ImGui::Text("Functions");
  ImGui::Separator();

  for (const auto &func : decompilerCtx_->GetFunctions()) {
    bool isSelected = (selectedFunction_ == func.get());
    if (ImGui::Selectable(func->name.c_str(), isSelected)) {
      selectedFunction_ = func.get();
    }
  }

  if (decompilerCtx_->GetFunctions().empty()) {
    ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "No functions found.");
    if (!lastPkg_) {
      ImGui::TextWrapped("Load a PKG to start analysis.");
    }
  }

  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel: Code View
  ImGui::BeginChild("CodeView", ImVec2(0, 0), true);

  if (selectedFunction_) {
    ImGui::Text("Address: 0x%llX", selectedFunction_->address);
    ImGui::SameLine();
    ImGui::TextColored(
        ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[%s]",
        selectedFunction_->category ==
                ShadPKG::Decompiler::IR::Function::Category::GameLogic
            ? "GameLogic"
            : "Unknown");
    ImGui::Separator();

    ImGui::BeginTabBar("CodeTabs");
    if (ImGui::BeginTabItem("Disassembly")) {
      ImGui::BeginChild("DisasmScroll");
      for (const auto &bb : selectedFunction_->basicBlocks) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f),
                           "[BLOCK 0x%llX - 0x%llX]", bb->startAddress,
                           bb->endAddress);
        ImGui::Separator();

        for (const auto &instr : bb->instructions) {
          ImGui::Text("  0x%llX  %s", instr.address, instr.disassembly.c_str());
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Opcode: %d", (int)instr.opcode);
          }
        }

        if (!bb->successors.empty()) {
          ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                             "  -> Successors: ");
          ImGui::SameLine();
          for (auto succ : bb->successors) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "0x%llX ", succ);
            ImGui::SameLine();
          }
          ImGui::NewLine();
        }
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Pseudocode")) {
      ImGui::BeginChild("PseudoScroll");

      // Check if we need to regenerate code
      if (codeDirty_) {
        // Trigger regeneration to update names/structure
        decompilerCtx_->GenerateStructuredCode();
        codeDirty_ = false;
      }

      const auto &tokens =
          decompilerCtx_->GetFunctionTokens(selectedFunction_->address);

      if (tokens.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "// No code generated or function empty.");
      } else {
        bool firstOnLine = true;
        for (const auto &token : tokens) {
          // Determine Color
          ImVec4 color(0.9f, 0.9f, 0.9f, 1.0f); // Default white
          switch (token.type) {
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::Keyword:
            color = ImVec4(0.3f, 0.6f, 1.0f, 1.0f); // Blueish
            break;
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::Type:
            color = ImVec4(0.3f, 0.8f, 0.6f, 1.0f); // Greenish-Cyan
            break;
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::Function:
            color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // Yellow
            break;
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::Global:
            color = ImVec4(0.8f, 0.5f, 1.0f, 1.0f); // Purple
            break;
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::String:
            color = ImVec4(1.0f, 0.6f, 0.3f, 1.0f); // Orange
            break;
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::Number:
            color = ImVec4(0.6f, 1.0f, 0.6f, 1.0f); // Light Green
            break;
          case ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::Comment:
            color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); // Grey
            break;
          default:
            break;
          }

          // Handle Newlines explicitly if they are in text, or assume tokens
          // flow The CppEmitter likely puts newlines as Text tokens or part of
          // them. We'll iterate chars if necessary, but assuming clean tokens
          // for now. If token text contains \n, we should probably print and
          // reset line.

          // Interaction Check (before printing to capture cursor pos?)
          // ImGui operates on the "last item". So we print, then check.

          if (!firstOnLine)
            ImGui::SameLine(0, 0); // No extra spacing, let tokens control it
                                   // (or add small spacing?)
          // Actually, code tokens usually need spacing unless they are
          // punctuation. CppEmitter should ideally include whitespace in tokens
          // or we add it. For now, let's assume tokens include necessary
          // whitespace or we play safe. BETTER: CppEmitter usually emits "int"
          // then " " then "main". If not, we might squash things. Let's try 0
          // spacing first.

          ImGui::TextColored(color, "%s", token.text.c_str());

          if (ImGui::IsItemHovered()) {
            if (token.relatedAddress != 0) {
              ImGui::SetTooltip("Address: 0x%llX", token.relatedAddress);
              ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
          }

          // Click Navigation
          if (token.relatedAddress != 0 && ImGui::IsItemClicked(0)) {
            auto targetFunc =
                decompilerCtx_->GetFunctionAt(token.relatedAddress);
            if (targetFunc) {
              selectedFunction_ = targetFunc.get();
            }
          }

          // Context Menu (Renaming & Retyping)
          if (ImGui::IsItemClicked(1)) {
            if (token.type ==
                ShadPKG::Decompiler::Codegen::CppEmitter::TokenType::
                    Identifier) { // Assuming Identifier is used for vars
              // Check if it matches a local variable
              bool isLocal = false;
              if (selectedFunction_) {
                for (const auto &local : selectedFunction_->locals) {
                  if (local.name == token.text) {
                    isLocal = true;
                    break;
                  }
                }
              }

              if (isLocal) {
                isRetyping_ = true;
                variableToRetype_ = token.text;
                retypeSearchBuffer_.resize(256, '\0');
                ImGui::OpenPopup("RetypePopup");
              }
            } else if (token.relatedAddress != 0) {
              renameAddress_ = token.relatedAddress;
              // Pre-fill buffer with current name
              std::string currentName =
                  decompilerCtx_->GetSymbolDatabase()->getSymbolName(
                      renameAddress_);
              strncpy(renameBuffer_, currentName.c_str(),
                      sizeof(renameBuffer_) - 1);
              ImGui::OpenPopup("RenameContextPopup");
            }
          }

          // Check for newline in text to reset firstOnLine
          if (token.text.find('\n') != std::string::npos) {
            firstOnLine = true;
          } else {
            firstOnLine = false;
          }
        }
      }

      // Rename Popup
      if (ImGui::BeginPopup("RenameContextPopup")) {
        ImGui::Text("Rename Symbol at 0x%llX", renameAddress_);
        if (ImGui::IsWindowAppearing())
          ImGui::SetKeyboardFocusHere();

        if (ImGui::InputText("Name", renameBuffer_, sizeof(renameBuffer_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
          decompilerCtx_->GetSymbolDatabase()->renameSymbol(renameAddress_,
                                                            renameBuffer_);
          codeDirty_ = true;
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Apply")) {
          decompilerCtx_->GetSymbolDatabase()->renameSymbol(renameAddress_,
                                                            renameBuffer_);
          codeDirty_ = true;
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      // Retype Popup
      DrawRetypePopup();

      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();

  } else {
    ImGui::Text("Select a function to view code.");
  }

  ImGui::EndChild();

  ImGui::EndGroup();
}

void DecompilerView::DrawRetypePopup() {
  if (ImGui::BeginPopup("RetypePopup")) {
    ImGui::Text("Retype Variable: %s", variableToRetype_.c_str());

    ImGui::Text("Search Type:");
    ImGui::InputText("##search", &retypeSearchBuffer_[0], 256);
    std::string search = retypeSearchBuffer_.c_str(); // Naive

    ImGui::Separator();

    ImGui::BeginChild("TypeList", ImVec2(300, 200), true);

    auto typeManager = decompilerCtx_->GetTypeManager();
    auto allTypes = typeManager->getAllTypes();

    // Add Primitives manual or via GetAllTypes if it includes them?
    // Assuming GetAllTypes includes logic or we add basic ones.
    // If TypeManager only has Structs in 'namedTypes_', we need to ensure
    // primitives are there. For now, let's list common primitives explicitly or
    // assume TypeManager handles them.

    std::vector<std::string> typesToShow;
    typesToShow.push_back("int8");
    typesToShow.push_back("int16");
    typesToShow.push_back("int32");
    typesToShow.push_back("int64");
    typesToShow.push_back("float");
    typesToShow.push_back("double");

    for (const auto &[name, type] : allTypes) {
      typesToShow.push_back(name);
    }

    for (const auto &name : typesToShow) {
      bool matches = search.empty() || name.find(search) != std::string::npos;
      if (matches) {
        if (ImGui::Selectable(name.c_str())) {
          // APPLY TYPE
          // Find local variable
          for (auto &local : selectedFunction_->locals) {
            if (local.name == variableToRetype_) {
              // If user picked a struct, we need to know if it's Pointer or
              // Value. GUI simplification: If existing type was pointer-like or
              // var usage implies pointer, prefer pointer? Or show "T" and "T*"
              // options.

              // Simple logic: Just set the type. If name matches a struct,
              // create struct type. But wait, TypeManager returns a Type
              // shared_ptr.

              auto type = typeManager->getType(name);
              if (type) {
                // Default to Pointer for Structs if not specified?
                // User request: "Gestisci i puntatori: permetti all'utente di
                // specificare se è T o T*" For now, defaulting to T* is safer
                // for decompilation of objects.

                if (type->getKind() ==
                    ShadPKG::Decompiler::Analysis::Type::Kind::Struct) {
                  // Wrap in Pointer
                  local.complexType = std::make_shared<
                      ShadPKG::Decompiler::Analysis::PointerType>(type);
                } else {
                  local.complexType = type;
                }
                codeDirty_ = true;
              }
              ImGui::CloseCurrentPopup();
              break;
            }
          }
        }

        // Add right-click for "As Pointer" vs "As Value" maybe?
        if (ImGui::IsItemClicked(1)) {
          // Advanced selection
        }
      }
    }

    ImGui::EndChild();
    ImGui::EndPopup();
  }
}

} // namespace ShadPKG::GUI
