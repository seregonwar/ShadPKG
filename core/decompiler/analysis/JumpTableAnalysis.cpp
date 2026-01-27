#include "JumpTableAnalysis.h"
#include "../DecompilerContext.h"
#include <capstone/capstone.h>
#include <iostream>
#include <map>

namespace ShadPKG::Decompiler::Analysis {

JumpTableAnalysis::JumpTableAnalysis(std::shared_ptr<IR::Function> func)
    : func_(func) {}

void JumpTableAnalysis::analyze() {
  if (!func_)
    return;

  std::cout << "[JumpTableAnalysis] Analyzing function at " << std::hex
            << func_->address << std::dec << std::endl;

  // Initialize Capstone
  csh handle;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
    return;
  }
  cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

  // Build ID map
  std::map<uint64_t, uint64_t> addrToId;
  for (const auto &bb : func_->basicBlocks) {
    if (bb)
      addrToId[bb->startAddress] = bb->id;
  }

  // Iterate basic blocks
  for (auto &block : func_->basicBlocks) {
    if (block->instructions.empty())
      continue;

    const auto &lastInst = block->instructions.back();

    // Check if it's a JMP (via OpCode)
    if (lastInst.opcode == IR::OpCode::JMP) {
      // Re-disassemble ONLY the last instruction to get details
      auto &ctx = DecompilerContext::Get();
      const auto &data = ctx.GetRawData();
      uint64_t base = ctx.GetBaseAddress();

      if (lastInst.address < base)
        continue;
      uint64_t offset = lastInst.address - base;
      if (offset + 16 > data.size())
        continue; // Safety check

      const uint8_t *code = &data[offset];
      size_t size = 16; // Max x86 inst length
      uint64_t addr = lastInst.address;

      cs_insn *insn;
      size_t count = cs_disasm(handle, code, size, addr, 1, &insn);
      if (count > 0) {
        // Check operands
        if (insn[0].detail->x86.op_count > 0) {
          const auto &op = insn[0].detail->x86.operands[0];
          if (op.type == X86_OP_MEM && op.mem.index != X86_REG_INVALID) {
            // Found Jump Table Pattern
            std::cout << "[JumpTableAnalysis] Found potential indirect jump at "
                      << std::hex << addr << std::dec << std::endl;

            uint64_t tableBase = op.mem.disp;
            std::vector<uint64_t> targets;

            for (int i = 0; i < 256; ++i) {
              uint64_t entryAddr = tableBase + i * op.mem.scale;
              uint64_t target = 0;

              if (op.mem.scale == 8)
                target = readPointer(entryAddr);
              else if (op.mem.scale == 4) {
                int32_t off = readInt32(entryAddr);
                if (off)
                  target = static_cast<uint64_t>(off);
              } else
                break;

              if (target == 0)
                break;
              targets.push_back(target);

              // Update CFG
              if (addrToId.count(target)) {
                uint64_t targetId = addrToId[target];

                // Avoid duplicates
                bool exists = false;
                for (auto s : block->successors)
                  if (s == targetId)
                    exists = true;
                if (!exists)
                  block->successors.push_back(targetId);

                // Populate Switch Map (Value -> Block ID)
                // Assuming 'i' is the case value (index)
                block->switchMap[i] = targetId;
              }
              targets.push_back(target);

              // Update CFG
              if (addrToId.count(target)) {
                uint64_t targetId = addrToId[target];
                // Avoid duplicates
                bool exists = false;
                for (auto s : block->successors)
                  if (s == targetId)
                    exists = true;
                if (!exists)
                  block->successors.push_back(targetId);
              }
            }
            std::cout << "  -> Found " << targets.size() << " targets."
                      << std::endl;
          }
        }
        cs_free(insn, count);
      }
    }
  }
  cs_close(&handle);
}

uint64_t JumpTableAnalysis::readPointer(uint64_t address) {
  return DecompilerContext::Get().ReadPointer(address);
}

int32_t JumpTableAnalysis::readInt32(uint64_t address) {
  return DecompilerContext::Get().ReadInt32(address);
}

} // namespace ShadPKG::Decompiler::Analysis
