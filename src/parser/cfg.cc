#include "../../include/parser/cfg.hpp"

namespace mylang {
namespace parser {

void ControlFlowGraph::addBlock(BasicBlock block)
{
    Blocks_.push_back(std::move(block));
}

void ControlFlowGraph::addEdge(int32_t from, int32_t to)
{
    if (from >= 0 && from < Blocks_.size() && to >= 0 && to < Blocks_.size()) {
        Blocks_[from].successors.push_back(to);
        Blocks_[to].predecessors.push_back(from);
    }
}

// Compute reachability
void ControlFlowGraph::computeReachability()
{
    if (Blocks_.empty())
        return;

    Blocks_[EntryBlock_].isReachable = true;
    std::vector<int32_t> worklist = { EntryBlock_ };

    while (!worklist.empty()) {
        int32_t blockId = worklist.back();
        worklist.pop_back();

        for (int32_t succ : Blocks_[blockId].successors) {
            if (!Blocks_[succ].isReachable) {
                Blocks_[succ].isReachable = true;
                worklist.push_back(succ);
            }
        }
    }
}

// Liveness analysis for dead code detection
void ControlFlowGraph::computeLiveness()
{
    bool changed = true;

    while (changed) {
        changed = false;

        for (int32_t i = Blocks_.size() - 1; i >= 0; --i) {
            ControlFlowGraph::BasicBlock& block = Blocks_[i];

            // liveOut = union of successors' liveIn
            std::unordered_set<StringRef, StringRefHash, StringRefEqual> newLiveOut;
            for (int32_t succ : block.successors)
                newLiveOut.insert(Blocks_[succ].liveIn.begin(), Blocks_[succ].liveIn.end());

            // liveIn = use ∪ (liveOut - def)
            std::unordered_set<StringRef, StringRefHash, StringRefEqual> newLiveIn = block.useVars;
            for (StringRef const& var : newLiveOut)
                if (!block.defVars.count(var))
                    newLiveIn.insert(var);

            if (newLiveIn != block.liveIn || newLiveOut != block.liveOut) {
                block.liveIn = std::move(newLiveIn);
                block.liveOut = std::move(newLiveOut);
                changed = true;
            }
        }
    }
}

std::vector<int32_t> ControlFlowGraph::getUnreachableBlocks() const
{
    std::vector<int32_t> unreachable;
    for (size_t i = 0, n = Blocks_.size(); i < n; ++i)
        if (!Blocks_[i].isReachable)
            unreachable.push_back(i);

    return unreachable;
}

std::vector<typename ControlFlowGraph::BasicBlock> const& ControlFlowGraph::getBlocks() const
{
    return Blocks_;
}

}
}
