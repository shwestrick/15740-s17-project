#include <sstream>
#include "dynamic_part_repl_policies.h"

DynamicWayPartReplPolicy::DynamicWayPartReplPolicy(
    uint64_t _numLines,
    uint32_t _numWays,
    DynamicPartMapper *_mapper,
    ReplPolicy* _delegateReplPolicy)
        : numPartitions(_mapper->getNumPartitions())
        , numLines(_numLines)
        , numWays(_numWays)
        , waySize(numLines / numWays)
        , partitionSizes(numPartitions, 0)
        , delegateReplPolicy(_delegateReplPolicy)
        , mapper(_mapper) {
    assert(numLines % numWays == 0);
    assert(numPartitions < numWays);

    info("DynamicWayPartReplPolicy with %u partitions and %u ways, using repl %p",
         numPartitions, numWays, delegateReplPolicy);

    partitionSizes[0] = numWays / numPartitions;
    assert(partitionSizes[0] > 1);
    for (uint32_t p = 1; p <= numPartitions; p++) {
        partitionSizes[p] = ((p * numWays) / numPartitions) - partitionSizes[p-1];
    }
}

DynamicWayPartReplPolicy::~DynamicWayPartReplPolicy() {
}

void DynamicWayPartReplPolicy::update(
    uint32_t id,
    const MemReq* req) {
    delegateReplPolicy->update(id, req);
}

void DynamicWayPartReplPolicy::replaced(
    uint32_t id) {
    delegateReplPolicy->replaced(id);
}

template<class C> inline
uint32_t DynamicWayPartReplPolicy::rank(
    const MemReq* req,
    C cands) {

    assert(cands.size() == numWays);

    uint32_t partition = mapper->getPartition(*req);

    // count how many candidates exist from each partition
    uint32_t numPartCands[numPartitions];
    for (uint32_t p = 0; p < numPartitions; p++) {
        numPartCands[p] = 0;
    }

    for (auto iter = cands.begin(); iter != cands.end(); iter++) {
        auto id = *iter;
        auto part = mapper->getPartition(id);
        numPartCands[part]++;
    }

    // if one partition has exceeded its allocation, replace from it,
    // otherwise replace from the requesting partition
    uint32_t victimPartition = -1u;

    for (uint32_t p = 0; p < numPartitions; p++) {
        if (numPartCands[p] > partitionSizes[p]) {
            victimPartition = p;
            break;
        }
    }

    if (victimPartition == -1u) {
        victimPartition = partition;
    }

    // now build the list of candidates and replace one of them
    uint32_t candsArray[numWays];
    uint32_t candsLength = 0;

    for (auto iter = cands.begin(); iter != cands.end(); iter++) {
        auto id = *iter;
        if (mapper->getPartition(id) == victimPartition) {
            candsArray[candsLength] = id;
            candsLength++;
        }
    }

    return delegateReplPolicy->rankCands(req, ArrayCands(candsArray, candsLength));
}

void DynamicWayPartReplPolicy::setPartitionSizes(
    const uint32_t* sizes) {
    
    std::stringstream s;
    s << "Partition sizes: ";
    for (uint32_t p = 0; p < numPartitions; p++) {
        partitionSizes[p] = sizes[p];
        s << sizes[p] << " ";
    }
    info("%s", s.str().c_str());
}

uint32_t DynamicWayPartReplPolicy::getNumBuckets() const {
    return numWays;
}

void DynamicWayPartReplPolicy::initStats(AggregateStat* parentStat) {
    delegateReplPolicy->initStats(parentStat);
}
