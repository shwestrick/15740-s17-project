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

#ifndef ASYM_RRIP_REPL_POLICIES_H_
#define ASYM_RRIP_REPL_POLICIES_H_

#include "repl_policies.h"

class AsymSRRIPReplPolicy : public virtual ReplPolicy {
        static const uint64_t UPDATE_INTERVAL = 256000;
        uint64_t rhits, whits, accsUntilUpdate;
    protected:
        inline void monitor(bool hit, bool write) {
            if (hit) {
                if (write){
                    ++whits;
                }else{
                    ++rhits;
                }
            }
            if (--accsUntilUpdate == 0) {
                info("RHits: %lu / %lu = %.3g", rhits, UPDATE_INTERVAL, 1. * rhits / UPDATE_INTERVAL);
                info("WHits: %lu / %lu = %.3g", whits, UPDATE_INTERVAL, 1. * whits / UPDATE_INTERVAL);
                accsUntilUpdate = UPDATE_INTERVAL;
                rhits = 0;
                whits = 0;
            }
        }
        
        VectorCounter profDistrib, profEvictionDistrib;
        Counter profInsertions, profPromotions, profDemotions;

        uint32_t* prio_array;
        bool* dirty_array;
        const uint32_t numLines;
        const uint32_t vmax;
        const uint32_t rstart;
        const uint32_t wstart;
        const uint32_t cprom;
        const uint32_t dprom;
        MTRand rnd;

        virtual uint32_t prioToRank(uint32_t id, uint32_t prio) {
            return prio;
        }
        
    public:
        AsymSRRIPReplPolicy(uint32_t _numLines, uint32_t M, uint32_t _rstart, uint32_t _wstart, uint32_t _cprom, uint32_t _dprom)
                    : numLines(_numLines), vmax(1 << M), rnd(4242), rstart(_rstart), wstart(_wstart), cprom(_cprom), dprom(_dprom) {
            hits = 0;
            accsUntilUpdate = UPDATE_INTERVAL;
            
            prio_array = gm_calloc<uint32_t>(numLines);
            dirty_array = gm_calloc<bool>(numLines);
            // In normal SRRIP, values go from 0-2^M-1. We do 0-2^M, reserve 0 for unused blocks, and invert the priority scheme (higher value is higher priority)
            assert(vmax > 1); //otherwise this is useless...
            assert(vmax >= rstart);
            assert(vmax >= wstart);
            assert(vmax >= cprom);
            assert(vmax >= dprom);
        }

        void initStats(AggregateStat* parent) {
            AggregateStat* rpStat = new AggregateStat();
            rpStat->init("repl", "RRIP replacement policy stats");
            initBaseStats(rpStat);
            parent->append(rpStat);
        }

        void update(uint32_t id, const MemReq* req) {
            bool write;
            //check to see if the access is a write or a read
            //It might be worth trying to optimize by storing a GETX request as if it werea write
            //Do we need to do anything with a PUTS request?

            //Option 1: treat GET as read and PUT as write
            if (req->type == GETS || req->type == GETX){
                write = false;
            }else if (req->type == PUTS || req->type == PUTX){
                write = true;
            }else{ assert(false); }
            //Option 2: treat S as read and X as write
/*            if (req->type == GETS || req->type == PUTS){
                write = false;
            }else if (req->type == GETX || req->type == PUTX){
                write = true;
            }else{ assert(false); }*/

            monitor(prio_array[id] != 0, write);
            updateDirty(id, write);

            if (prio_array[id] == 0) {
                if (write){
                    changePrio(id, wstart); //predict long re-reference
                }else{
                    changePrio(id, rstart);
                }
                profInsertions.inc();
            } else {
                bool dirty = dirty_array[id];
                //This needs to check if the item is currently dirty or is being written to, not just the latter
                if (dirty){
                    changePrio(id, wprom); //predict near-immediate re-reference
                else{
                    changePrio(id, cprom);
                }
                profPromotions.inc();
            }
        }

        void replaced(uint32_t id) {
            changePrio(id, 0);
            clearDirty(id);
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

        DECL_RANK_BINDINGS();

    protected:
        inline void changePrio(uint32_t id, uint32_t newPrio) {
            profDistrib.inc(array[id], -1);
            profDistrib.inc(newPrio);
            array[id] = newPrio;
        }

        inline void updateDirty(uint32_t id, bool write){
            array[id] = array[id] || write;
        }

        inline void clearDirty(uint32_t id){
            array[id] = false;
        }

        virtual void initBaseStats(AggregateStat* rpStat) {
            profInsertions.init("ins", "Insertions"); rpStat->append(&profInsertions);
            profPromotions.init("proms", "Promotions"); rpStat->append(&profPromotions);
            profDemotions.init("dems", "Demotions"); rpStat->append(&profDemotions);
            profEvictionDistrib.init("evDistrib", "Prio distrib of evicted blocks (0-vmax, 0 is unused lines)", vmax+1); rpStat->append(&profEvictionDistrib);
            profDistrib.init("distrib", "Prio distrib of blocks (0-vmax, 0 is unused lines)", vmax+1); profDistrib.inc(0, numLines); rpStat->append(&profDistrib);
        }
};

#endif
