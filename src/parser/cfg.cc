#include "../../include/parser/cfg.hpp"


namespace mylang {
namespace parser {

void ControlFlowGraph::addBlock(BasicBlock block) { blocks_.push_back(std::move(block)); }

void ControlFlowGraph::addEdge(std::int32_t from, std::int32_t to)
{
  if (from >= 0 && from < blocks_.size() && to >= 0 && to < blocks_.size())
  {
    blocks_[from].successors.push_back(to);
    blocks_[to].predecessors.push_back(from);
  }
}

// Compute reachability
void ControlFlowGraph::computeReachability()
{
  if (blocks_.empty()) return;

  blocks_[entryBlock_].isReachable = true;
  std::vector<std::int32_t> worklist = {entryBlock_};

  while (!worklist.empty())
  {
    std::int32_t blockId = worklist.back();
    worklist.pop_back();

    for (std::int32_t succ : blocks_[blockId].successors)
    {
      if (!blocks_[succ].isReachable)
      {
        blocks_[succ].isReachable = true;
        worklist.push_back(succ);
      }
    }
  }
}

// Liveness analysis for dead code detection
void ControlFlowGraph::computeLiveness()
{
  bool changed = true;
  while (changed)
  {
    changed = false;

    for (std::int32_t i = blocks_.size() - 1; i >= 0; --i)
    {
      auto& block = blocks_[i];
      // liveOut = union of successors' liveIn
      std::unordered_set<string_type> newLiveOut;
      for (std::int32_t succ : block.successors)
        newLiveOut.insert(blocks_[succ].liveIn.begin(), blocks_[succ].liveIn.end());
      // liveIn = use ∪ (liveOut - def)
      std::unordered_set<string_type> newLiveIn = block.useVars;
      for (const string_type& var : newLiveOut)
        if (!block.defVars.count(var)) newLiveIn.insert(var);
      if (newLiveIn != block.liveIn || newLiveOut != block.liveOut)
      {
        block.liveIn = std::move(newLiveIn);
        block.liveOut = std::move(newLiveOut);
        changed = true;
      }
    }
  }
}

std::vector<std::int32_t> ControlFlowGraph::getUnreachableBlocks() const
{
  std::vector<std::int32_t> unreachable;
  for (std::size_t i = 0; i < blocks_.size(); ++i)
    if (!blocks_[i].isReachable) unreachable.push_back(i);
  return unreachable;
}

const std::vector<typename ControlFlowGraph::BasicBlock>& ControlFlowGraph::getBlocks() const { return blocks_; }

}
}