#include "../../include/parser/cfg.hpp"


namespace mylang {
namespace parser {

void ControlFlowGraph::addBlock(BasicBlock block) { Blocks_.push_back(std::move(block)); }

void ControlFlowGraph::addEdge(std::int32_t from, std::int32_t to)
{
  if (from >= 0 && from < Blocks_.size() && to >= 0 && to < Blocks_.size())
  {
    Blocks_[from].successors.push_back(to);
    Blocks_[to].predecessors.push_back(from);
  }
}

// Compute reachability
void ControlFlowGraph::computeReachability()
{
  if (Blocks_.empty())
    return;

  Blocks_[EntryBlock_].IsReachable   = true;
  std::vector<std::int32_t> worklist = {EntryBlock_};

  while (!worklist.empty())
  {
    std::int32_t blockId = worklist.back();
    worklist.pop_back();

    for (std::int32_t succ : Blocks_[blockId].successors)
    {
      if (!Blocks_[succ].IsReachable)
      {
        Blocks_[succ].IsReachable = true;
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

    for (std::int32_t i = Blocks_.size() - 1; i >= 0; --i)
    {
      ControlFlowGraph::BasicBlock& block = Blocks_[i];

      // liveOut = union of successors' liveIn
      std::unordered_set<StringType> newLiveOut;
      for (std::int32_t succ : block.successors)
        newLiveOut.insert(Blocks_[succ].LiveIn.begin(), Blocks_[succ].LiveIn.end());

      // liveIn = use ∪ (liveOut - def)
      std::unordered_set<StringType> newLiveIn = block.UseVars;
      for (const StringType& var : newLiveOut)
      {
        if (!block.DefVars.count(var))
          newLiveIn.insert(var);
      }

      if (newLiveIn != block.LiveIn || newLiveOut != block.LiveOut)
      {
        block.LiveIn  = std::move(newLiveIn);
        block.LiveOut = std::move(newLiveOut);
        changed       = true;
      }
    }
  }
}

std::vector<std::int32_t> ControlFlowGraph::getUnreachableBlocks() const
{
  std::vector<std::int32_t> unreachable;
  for (std::size_t i = 0; i < Blocks_.size(); ++i)
  {
    if (!Blocks_[i].IsReachable)
      unreachable.push_back(i);
  }
  return unreachable;
}

const std::vector<typename ControlFlowGraph::BasicBlock>& ControlFlowGraph::getBlocks() const { return Blocks_; }

}
}