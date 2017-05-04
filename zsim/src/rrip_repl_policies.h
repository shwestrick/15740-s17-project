/** $lic$
 * Copyright (C) 2012-2013 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2012 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * This is an internal version, and is not under GPL. All rights reserved.
 * Only MIT and Stanford students and faculty are allowed to use this version.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2010) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

#ifndef RRIP_REPL_POLICIES_H_
#define RRIP_REPL_POLICIES_H_

#include "repl_policies.h"

class SRRIPReplPolicy : public virtual ReplPolicy {
        static const uint64_t UPDATE_INTERVAL = 256000;
        uint64_t hits, accsUntilUpdate;
    protected:
        inline void monitor(bool hit) {
            if (hit) { ++hits; }
            if (--accsUntilUpdate == 0) {
                info("Hits: %lu / %lu = %.3g", hits, UPDATE_INTERVAL, 1. * hits / UPDATE_INTERVAL);
                accsUntilUpdate = UPDATE_INTERVAL;
                hits = 0;
            }
        }
        
        VectorCounter profDistrib, profEvictionDistrib;
        Counter profInsertions, profPromotions, profDemotions;

        uint32_t* array;
        const uint32_t numLines;
        const uint32_t vmax;
        MTRand rnd;

        virtual uint32_t prioToRank(uint32_t id, uint32_t prio) {
            return prio;
        }
        
    public:
        SRRIPReplPolicy(uint32_t _numLines, uint32_t M) : numLines(_numLines), vmax(1 << M), rnd(4242) {
            hits = 0;
            accsUntilUpdate = UPDATE_INTERVAL;
            
            array = gm_calloc<uint32_t>(numLines);
            // In normal SRRIP, values go from 0-2^M-1. We do 0-2^M, reserve 0 for unused blocks, and invert the priority scheme (higher value is higher priority)
            assert(vmax > 1); //otherwise this is useless...
        }

        void initStats(AggregateStat* parent) {
            AggregateStat* rpStat = new AggregateStat();
            rpStat->init("repl", "RRIP replacement policy stats");
            initBaseStats(rpStat);
            parent->append(rpStat);
        }

        void update(uint32_t id, const MemReq* req) {
            monitor(array[id] != 0);
            
            if (array[id] == 0) {
                changePrio(id, 2); //predict long re-reference
                profInsertions.inc();
            } else {
                changePrio(id, vmax); //predict near-immediate re-reference
                profPromotions.inc();
            }
        }

        void replaced(uint32_t id) {
            changePrio(id, 0);
        }

        template <typename C> uint32_t rank(const MemReq* req, C cands) {
            uint32_t numCandidates = cands.size();
            uint32_t bestCands[numCandidates];
            uint32_t bestPrio = (uint32_t)-1;
            uint32_t bestRank = (uint32_t)-1;
            uint32_t pos = 0;

            for (auto ci = cands.begin(); ci != cands.end(); ci++) {
                uint32_t c = *ci;
                uint32_t prio = array[c];
                uint32_t r = prioToRank(c, prio);
                if (r == bestRank) {
                    bestCands[++pos] = c;
                } else if (r < bestRank) {
                    bestPrio = prio;
                    bestRank = r;
                    pos = 0;
                    bestCands[0] = c;
                }
            }
            assert(bestPrio != (uint32_t)-1);

            //Age cands if needed
            if (bestPrio > 1) {
                uint32_t aging = bestPrio - 1;
                for (auto ci = cands.begin(); ci != cands.end(); ci++) {
                    uint32_t c = *ci;
                    if (array[c] > aging) { // zcaches can give duplicate candidates, avoid overaging
                        changePrio(c, array[c] - aging);
                        profDemotions.inc();
                    }
                }
            }

            profEvictionDistrib.inc(bestPrio);
            return bestCands[rnd.randInt(pos)]; //break ties randomly
        }

        DECL_RANK_BINDINGS;

    protected:
        inline void changePrio(uint32_t id, uint32_t newPrio) {
            profDistrib.inc(array[id], -1);
            profDistrib.inc(newPrio);
            array[id] = newPrio;
        }

        virtual void initBaseStats(AggregateStat* rpStat) {
            profInsertions.init("ins", "Insertions"); rpStat->append(&profInsertions);
            profPromotions.init("proms", "Promotions"); rpStat->append(&profPromotions);
            profDemotions.init("dems", "Demotions"); rpStat->append(&profDemotions);
            profEvictionDistrib.init("evDistrib", "Prio distrib of evicted blocks (0-vmax, 0 is unused lines)", vmax+1); rpStat->append(&profEvictionDistrib);
            profDistrib.init("distrib", "Prio distrib of blocks (0-vmax, 0 is unused lines)", vmax+1); profDistrib.inc(0, numLines); rpStat->append(&profDistrib);
        }
};

class BRRIPReplPolicy : public SRRIPReplPolicy {
    private:
        Counter profLowInsertions;

    public:
        BRRIPReplPolicy(uint32_t _numLines, uint32_t M) : SRRIPReplPolicy(_numLines, M) {}
        void update(uint32_t id, const MemReq* req) {
            monitor(array[id] != 0);

            if (array[id] == 0) {
                if (rnd.randInt(32) == 0) {
                    changePrio(id, 2);
                } else {
                    changePrio(id, 1);
                    profLowInsertions.inc();
                }
                profInsertions.inc();
            } else {
                changePrio(id, vmax); //predict near-immediate re-reference
                profPromotions.inc();
            }
        }

    protected:
        virtual void initBaseStats(AggregateStat* rpStat) {
            profLowInsertions.init("loIns", "Low-prio insertions (BRRIP)"); rpStat->append(&profLowInsertions);
            SRRIPReplPolicy::initBaseStats(rpStat);
        }
};

class AuxTagDir : public GlobAlloc {
    public:
        CacheArray* atd;

        explicit AuxTagDir(CacheArray* _atd) : atd(_atd) {}
        bool access(const MemReq* req, uint32_t* outLineId = NULL) {
            int32_t lineId = atd->lookup(req->lineAddr, req, true);
            if (lineId == -1) {
                Address wbLineAddr;
                lineId = atd->preinsert(req->lineAddr, req, &wbLineAddr);
                atd->postinsert(req->lineAddr, req, lineId);
                if (outLineId) *outLineId = lineId;
                return false;
            } else {
                if (outLineId) *outLineId = lineId;
                return true;
            }
        }
};

class PolicyDueler : public GlobAlloc {
    private:
        uint32_t samplingFactor;
        uint32_t atdLines;
        uint32_t atdWays;
        HashFamily* atdHash;
        AuxTagDir atd1, atd2;
        HashFamily* samplingHash;
        int32_t pselCounter; //[-1024, 1023]; non-negative -> rp1 wins

    public:
        PolicyDueler(ReplPolicy* rp1, ReplPolicy* rp2, uint32_t cacheLines, uint32_t _samplingFactor) :
            samplingFactor(_samplingFactor), atdLines(cacheLines/samplingFactor), atdWays(16),
            atdHash(new H3HashFamily(1, ilog2(atdLines/atdWays), 0x100A7D00)),
	    atd1(new SetAssocArray(atdLines, atdWays, rp1, atdHash)),
	    atd2(new SetAssocArray(atdLines, atdWays, rp2, atdHash)),
            samplingHash(new H3HashFamily(1, 32, 0x200A7D00)), pselCounter(0)
        {
            assert(cacheLines % samplingFactor == 0);
            assert(isPow2(samplingFactor));
        }

        void access(const MemReq* req) {
            Address addr = req->lineAddr;
            if ((samplingHash->hash(0, addr) & (samplingFactor - 1)) == 0) {
                bool hit1 = atd1.access(req);
                bool hit2 = atd2.access(req);
                pselCounter = MIN(MAX(pselCounter + hit1 - hit2, -1024), 1023);
            }
        }

        inline bool useRP1() const { return pselCounter >= 0; }
};

class DRRIPReplPolicy : public BRRIPReplPolicy {
    private:
        uint32_t samplingFactor;
        ReplPolicy* rp1;
        ReplPolicy* rp2;
        PolicyDueler pd;

        Counter profSRRIPInsertions;

    public:
        DRRIPReplPolicy(uint32_t _numLines, uint32_t M) : BRRIPReplPolicy(_numLines, M), samplingFactor(128),
            rp1(new SRRIPReplPolicy(_numLines/samplingFactor, M)), rp2(new BRRIPReplPolicy(_numLines/samplingFactor, M)),
            pd(rp1, rp2, _numLines, samplingFactor)
        {
            //Fake out the ATD's replacement policy stats
            AggregateStat* fakeRootStat = new AggregateStat();
            fakeRootStat->init("dummy", "dummy");
            rp1->initStats(fakeRootStat);
            fakeRootStat->makeImmutable();
            fakeRootStat = new AggregateStat();
            fakeRootStat->init("dummy", "dummy");
            rp2->initStats(fakeRootStat);
            fakeRootStat->makeImmutable();
        }

        void update(uint32_t id, const MemReq* req) {
            pd.access(req);
            if (pd.useRP1()) {
                if (array[id] == 0) profSRRIPInsertions.inc();
                SRRIPReplPolicy::update(id, req);
            } else {
                BRRIPReplPolicy::update(id, req);
            }
        }

    protected:
        virtual void initBaseStats(AggregateStat* rpStat) {
            profSRRIPInsertions.init("srripIns", "SRRIP-mode insertions (DRRIP)"); rpStat->append(&profSRRIPInsertions);
            BRRIPReplPolicy::initBaseStats(rpStat);
        }
};

// This version of DRRIP actually runs SRRIP and BRRIP on parts of the
// cache to choose the policy for the remainder of the cache.
//
// Note: This assumes that the samplingFactor divides the space of
// line ids into cohesive regions. Lines from 0...(numLines /
// samplingFactor - 1) will use SRRIP. Lines (numLines /
// samplingFactor) ... (2 * numLines / samplingFactor - 1) will use
// BRRIP. This generally only makes sense with SetAssociative
// caches. If you are using skew-associative caches, then this
// sampling strategy makes little sense.
class DRRIPSetSamplingReplPolicy : public BRRIPReplPolicy {
    private:
        int32_t pselCounter;
        uint32_t sampledLines;

    public:
        DRRIPSetSamplingReplPolicy(
            uint32_t _numLines,
            uint32_t M,
            uint32_t _samplingFactor)
                : BRRIPReplPolicy(_numLines, M)
                , pselCounter(0)
                , sampledLines(_numLines / _samplingFactor) {
            assert(sampledLines > 0);
        }

        ~DRRIPSetSamplingReplPolicy() {
        }

        void update(uint32_t id, const MemReq* req) {
            if (id < sampledLines) {
                // monitor SRRIP
                if (array[id] == 0) {
                    // miss
                    pselCounter--;
                    pselCounter = MAX(pselCounter, -1024);
                } else {
                    pselCounter++;
                    pselCounter = MIN(pselCounter, 1023);
                }
                SRRIPReplPolicy::update(id, req);
            } else if (id < 2 * sampledLines) {
                // monitor BRRIP
                if (array[id] == 0) {
                    // miss
                    pselCounter++;
                    pselCounter = MIN(pselCounter, 1023);
                } else {
                    pselCounter--;
                    pselCounter = MAX(pselCounter, -1024);
                }
                BRRIPReplPolicy::update(id, req);
            } else {
                // rest of cache
                if (pselCounter >= 0) {
                    SRRIPReplPolicy::update(id, req);
                } else {
                    BRRIPReplPolicy::update(id, req);
                }
            }
        }
};

/* Thread-aware DRRIP (using TADIP-F, i.e., one dueling monitor per partition, each monitor uses the current decisions of others) */
class TADRRIPReplPolicy : public BRRIPReplPolicy {
    private:
        g_vector<PolicyDueler*> corePDs;
        g_vector<uint32_t> coreBRRIPs; // holds current policies for all, passes
        HashFamily* coreHash;

        VectorCounter profSRRIPInsertions;

        class DuelerPolicy : public BRRIPReplPolicy {
            private:
                const volatile uint32_t* otherCoreBRRIPs;  // 0 or 1, numcores size
                const uint32_t core;
                const bool useBRRIP;
            public:
                DuelerPolicy(uint32_t _numLines, uint32_t M, const volatile uint32_t* _coreBrrips, uint32_t _core, bool _useBRRIP) :
                    BRRIPReplPolicy(_numLines, M), otherCoreBRRIPs(_coreBrrips), core(_core), useBRRIP(_useBRRIP) {}

                void update(uint32_t id, const MemReq* req) {
                    bool brrip = (req->srcId == core)? useBRRIP : otherCoreBRRIPs[req->srcId];
                    if (brrip) BRRIPReplPolicy::update(id, req);
                    else SRRIPReplPolicy::update(id, req);
                }
        };

    public:
        TADRRIPReplPolicy(uint32_t _numLines, uint32_t M) : BRRIPReplPolicy(_numLines, M) {
            corePDs.resize(zinfo->numCores);
            coreBRRIPs.resize(zinfo->numCores);
            coreHash = new H3HashFamily(1, 32, 0x7AD07ADD);
            uint32_t samplingFactor = 128*zinfo->numCores;
            assert((_numLines % samplingFactor) == 0);
            assert((zinfo->numCores & (zinfo->numCores - 1)) == 0); //must be a power of 2 (actually, samplingFactor must...)

            //info("SF %d SL %d", samplingFactor, _numLines/samplingFactor);

            for (uint32_t c = 0; c < zinfo->numCores; c++) {
                DuelerPolicy* rp1 = new DuelerPolicy(_numLines/samplingFactor, M, (const volatile uint32_t*)&coreBRRIPs[0], c, true);
                DuelerPolicy* rp2 = new DuelerPolicy(_numLines/samplingFactor, M, (const volatile uint32_t*)&coreBRRIPs[0], c, false);

                // We already do a round of downsampling by downselecting externally, so divide BOTH numLines and sf passed to dueler
                // This way, we maintain the number of monitor lines = numLines/samplingFactor, but do the correct amount of filtering
                // (1/numCores in update() and samplingFactor/numCores in the dueler)
                corePDs[c] = new PolicyDueler(rp1, rp2, _numLines/zinfo->numCores, samplingFactor/zinfo->numCores);

                //Fake out the ATD's replacement policy stats
                AggregateStat* fakeRootStat = new AggregateStat();
                fakeRootStat->init("dummy", "dummy");
                rp1->initStats(fakeRootStat);
                fakeRootStat->makeImmutable();
                fakeRootStat = new AggregateStat();
                fakeRootStat->init("dummy", "dummy");
                rp2->initStats(fakeRootStat);
                fakeRootStat->makeImmutable();
            }
        }

        void update(uint32_t id, const MemReq* req) {
            // Split lines evenly across per-core SDMs (sampling factor accounts for this)
            assert(req->srcId < zinfo->numCores);
            uint32_t pdCore = ((coreHash->hash(0, req->lineAddr)*37) % 23471237) % zinfo->numCores;
            corePDs[pdCore]->access(req);
            coreBRRIPs[pdCore] = corePDs[pdCore]->useRP1();

            //info("pdCore %d [%d %d]", pdCore, coreBRRIPs[0], coreBRRIPs[1]);

            if (!coreBRRIPs[req->srcId]) {
                if (array[id] == 0) profSRRIPInsertions.inc(req->srcId);
                SRRIPReplPolicy::update(id, req);
            } else {
                BRRIPReplPolicy::update(id, req);
            }
        }

    protected:
        virtual void initBaseStats(AggregateStat* rpStat) {
            BRRIPReplPolicy::initBaseStats(rpStat);
            profSRRIPInsertions.init("srripIns", "Per-core SRRIP-mode insertions (TA-DRRIP)", zinfo->numCores);
            rpStat->append(&profSRRIPInsertions);
        }
};

// This version of TADRRIP actually runs SRRIP and BRRIP on parts of
// the cache to choose the policy for the remainder of the cache. See
// comment on DRRIPSetSamplingReplPolicy above.
class TADRRIPSetSamplingReplPolicy : public BRRIPReplPolicy {
    private:
        g_vector<int32_t> pselCounter;
        uint32_t sampledLines;
        PartMapper* mapper;

    public:
        TADRRIPSetSamplingReplPolicy(
            uint32_t _numLines,
            uint32_t M,
            uint32_t _samplingFactor)
                : BRRIPReplPolicy(_numLines, M)
                , pselCounter(zinfo->numCores, 0)
                , sampledLines(_numLines / _samplingFactor) {
            assert(sampledLines > 0);
            assert(sampledLines * 2 * zinfo->numCores < _numLines);
            info("TADRRIPSetSamplingReplPolicy: Using %g%% of the cache for sampling!",
                 1. * sampledLines * 2 * zinfo->numCores / _numLines);
        }

        ~TADRRIPSetSamplingReplPolicy() {
        }

        void update(uint32_t id, const MemReq* req) {
            uint32_t part = req->srcId; assert(part < zinfo->numCores);

            uint32_t samplingRegionStart = part * sampledLines;
            uint32_t srripRegionEnd = samplingRegionStart + sampledLines;
            uint32_t brripRegionEnd = srripRegionEnd + sampledLines;
            
            if (samplingRegionStart <= id && id < srripRegionEnd) {
                // monitor SRRIP
                if (array[id] == 0) {
                    // miss
                    pselCounter[part]--;
                    pselCounter[part] = MAX(pselCounter[part], -1024);
                } else {
                    pselCounter[part]++;
                    pselCounter[part] = MIN(pselCounter[part], 1023);
                }
                SRRIPReplPolicy::update(id, req);
            } else if (srripRegionEnd <= id && id < brripRegionEnd) {
                // monitor BRRIP
                if (array[id] == 0) {
                    // miss
                    pselCounter[part]++;
                    pselCounter[part] = MIN(pselCounter[part], 1023);
                } else {
                    pselCounter[part]--;
                    pselCounter[part] = MAX(pselCounter[part], -1024);
                }
                BRRIPReplPolicy::update(id, req);
            } else {
                // rest of cache
                if (pselCounter[part] >= 0) {
                    SRRIPReplPolicy::update(id, req);
                } else {
                    BRRIPReplPolicy::update(id, req);
                }
            }
        }
};

#endif  // RRIP_REPL_POLICIES_H_
