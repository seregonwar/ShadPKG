#include "gui/include/views/StructEditorView.h"
#include "gui/include/StyleManager.h"
#include "imgui.h"
#include <sstream>

namespace ShadPKG::GUI {

StructEditorView::StructEditorView() { newStructName_.resize(256); }

void StructEditorView::Draw(GUIContext &ctx) {
  ImGui::Begin("Struct Editor", nullptr, ImGuiWindowFlags_None);

  auto decompCtx = &ShadPKG::Decompiler::DecompilerContext::Get();
  auto typeManager = decompCtx->GetTypeManager();

  // 1. Struct List
  ImGui::BeginChild("StructList", ImVec2(200, 0), true);
  ImGui::Text("Defined Structs");
  ImGui::Separator();

  // Create New Struct
  if (ImGui::Button("New Struct")) {
    // Find unique name
    int i = 1;
    while (typeManager->getType("Struct" + std::to_string(i)))
      i++;
    typeManager->createStruct("Struct" + std::to_string(i));
  }

  ImGui::Separator();



  // Placeholder iteration (requires TypeManager update)
  auto allTypes = typeManager->getAllTypes(); // NEED TO IMPLEMENT
  int idx = 0;
  for (const auto &[name, type] : allTypes) {
    if (type->getKind() == ShadPKG::Decompiler::Analysis::Type::Kind::Struct) {
      bool selected = (selectedStructIndex_ == idx);
      if (ImGui::Selectable(name.c_str(), selected)) {
        selectedStructIndex_ = idx;
        newStructName_ = name;
        newStructName_.resize(256, '\0');
      }
      idx++;
    }
  }

  ImGui::EndChild();

  ImGui::SameLine();

  // 2. Struct Details Editor
  ImGui::BeginChild("StructDetails", ImVec2(0, 0), true);

  if (selectedStructIndex_ != -1) {
    // Find selected struct by index is inefficient but works for now
    // Requires stable ordering or index mapping.

    std::shared_ptr<ShadPKG::Decompiler::Analysis::StructType> currentStruct =
        nullptr;
    std::string currentName;

    int i = 0;
    for (const auto &[name, type] : allTypes) {
      if (type->getKind() ==
          ShadPKG::Decompiler::Analysis::Type::Kind::Struct) {
        if (i == selectedStructIndex_) {
          currentStruct = std::dynamic_pointer_cast<
              ShadPKG::Decompiler::Analysis::StructType>(type);
          currentName = name;
          break;
        }
        i++;
      }
    }

    if (currentStruct) {
      ImGui::Text("Editing: %s", currentName.c_str());

      // Rename
      if (ImGui::InputText("Name", &newStructName_[0], 256,
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newName = newStructName_.c_str();
        if (newName != currentName && !newName.empty()) {
          typeManager->renameType(currentName, newName); // NEED TO IMPLEMENT
        }
      }

      ImGui::Separator();
      ImGui::Text("Members");

      // Members Table
      if (ImGui::BeginTable("MembersTable", 4,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed,
                                60.0f);
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
                                50.0f);
        ImGui::TableHeadersRow();

        auto members = currentStruct->getMembers(); // Copy
        bool changed = false;

        for (int mIdx = 0; mIdx < members.size(); ++mIdx) {
          auto &member = members[mIdx];

          ImGui::PushID(mIdx);
          ImGui::TableNextRow();

          // Offset
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("0x%X", member.offset); // Read-only for now, or editable?

          // Type
          ImGui::TableSetColumnIndex(1);
          if (ImGui::Button(member.type->toString().c_str())) {
            ImGui::OpenPopup("TypeSelector");
          }
          if (ImGui::BeginPopup("TypeSelector")) {
            // List primitives
            if (ImGui::Selectable("int32")) {
              member.type = typeManager->getType("int32");
              changed = true;
            }
            if (ImGui::Selectable("float")) {
              member.type = typeManager->getType("float");
              changed = true;
            }
            // ... iterate all types
            ImGui::EndPopup();
          }

          // Name
          ImGui::TableSetColumnIndex(2);
          char nameBuf[64];
          strncpy(nameBuf, member.name.c_str(), 63);
          if (ImGui::InputText("##name", nameBuf, 64)) {
            member.name = nameBuf;
            changed = true;
          }

          // Action
          ImGui::TableSetColumnIndex(3);
          if (ImGui::Button("X")) {
            // Delete member
            // Need update logic
          }

          ImGui::PopID();
        }

        // Add new Member Row
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("+");
        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button("Add")) {
          currentStruct->addMember("field_" + std::to_string(members.size()),
                                   typeManager->getType("int32"),
                                   currentStruct->getSize());
        }

        ImGui::EndTable();

        if (changed) {
          // Update struct definition
          // currentStruct->setMembers(members); // NEED TO IMPLEMENT setter or
          // direct ref access
        }
      }
    }
  } else {
    ImGui::Text("Select a struct to edit.");
  }

  ImGui::EndChild();

  ImGui::End();
}

} // namespace ShadPKG::GUI
