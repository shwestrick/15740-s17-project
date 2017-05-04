#pragma once

#include "partition_mapper.h"
#include "dynamic_part_repl_policies.h"

class DirtyReplPolicy : public virtual ReplPolicy, public DynamicPartMapper {
  public:

    DirtyReplPolicy(uint32_t _numLines) {
    }

    bool isDirty(uint32_t id) {
        return cc->isDirty(id);
    }

    void initStats(AggregateStat* parent) {
        // todo...
    }

    // Part mapper interface
    uint32_t getNumPartitions() {
        return 2;                       // clean and dirty
    }

    static constexpr uint32_t CLEAN = 0;
    static constexpr uint32_t DIRTY = 1;

    uint32_t getPartition(const MemReq& req) {
        // this is only called for insertions, assume things are
        // inserted clean; this will be fixed during replacement when
        // we query the CC
        return CLEAN;
    }

    uint32_t getPartition(uint32_t id) {
        return isDirty(id)? DIRTY : CLEAN;
    }
};

class AsymmetricPartReplPolicy : public DirtyReplPolicy, public DynamicWayPartReplPolicy {
  public:

    AsymmetricPartReplPolicy(
        uint64_t _numLines,
        uint32_t _numWays,
        ReplPolicy* _delegateReplPolicy)
        : DirtyReplPolicy(_numLines)
        , DynamicWayPartReplPolicy(_numLines, _numWays, this, _delegateReplPolicy)
    {
    }
        
    template <class Cands>
    uint32_t rank(const MemReq* req, Cands cands) {
        return DynamicWayPartReplPolicy::rank<Cands>(req, cands);
    }

    DECL_RANK_BINDINGS;

    void initStats(AggregateStat* parent) {
        DirtyReplPolicy::initStats(parent);
        DynamicWayPartReplPolicy::initStats(parent);
    }

    void setCC(CC* _cc) {
        DirtyReplPolicy::setCC(_cc);
        DynamicWayPartReplPolicy::setCC(_cc);
    }
};

class StaticAsymmetricPartReplPolicy : public AsymmetricPartReplPolicy {
  public:
    
    StaticAsymmetricPartReplPolicy(
        uint64_t _numLines,
        uint32_t _numWays,
        uint32_t _dirtyWays,
        ReplPolicy* _delegateReplPolicy)
        : AsymmetricPartReplPolicy(_numLines, _numWays, _delegateReplPolicy)
    {
        info("Initializing static asymmetric partition repl policy with %lu lines, %u ways, and %u dirty ways",
             _numLines, _numWays, _dirtyWays);
        
        uint32_t partSizes[2];
        partSizes[CLEAN] = _numWays - _dirtyWays;
        partSizes[DIRTY] = _dirtyWays;
        DynamicWayPartReplPolicy::setPartitionSizes(partSizes);
    }
        
};
