// This file implements partitioning schemes where the partition can
// be a dynamic property of the access stream.

#pragma once

#include "part_repl_policies.h"

class DynamicWayPartReplPolicy : public virtual ReplPolicy {
  public:
    DynamicWayPartReplPolicy(
	uint64_t _numLines,
	uint32_t _numWays,
	DynamicPartMapper *_mapper,
	ReplPolicy* _delegateReplPolicy);

    ~DynamicWayPartReplPolicy();

    void update(
	uint32_t id,
	const MemReq* req);

    void replaced(
	uint32_t id);

    template<class C>
    uint32_t rank(
	const MemReq* req,
	C cands);

    void setPartitionSizes(
	const uint32_t* sizes);

    uint32_t getNumBuckets() const;

    void initStats(AggregateStat* parentStat);

    void setCC(CC* _cc) { delegateReplPolicy->setCC(_cc); }

    DECL_RANK_BINDINGS;

  private:
    uint32_t numPartitions;
    uint32_t numLines;
    uint32_t numWays;
    uint32_t waySize;
    g_vector<uint32_t> partitionSizes;
    ReplPolicy* delegateReplPolicy;
    DynamicPartMapper* mapper;
};
