/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "phase_concurrent_coherence_ctrls.h"
#include "cache.h"
#include "network.h"

/* Do a simple XOR block hash on address to determine its bank. Hacky for now,
 * should probably have a class that deals with this with a real hash function
 * (TODO)
 */
uint32_t DCWSOLIBottomCC::getParentId(Address lineAddr) {
    //Hash things a bit
    uint32_t res = 0;
    uint64_t tmp = lineAddr;
    for (uint32_t i = 0; i < 4; i++) {
        res ^= (uint32_t) ( ((uint64_t)0xffff) & tmp);
        tmp = tmp >> 16;
    }
    return (res % parents.size());
}


void DCWSOLIBottomCC::init(const g_vector<MemObject*>& _parents, Network* network, const char* name) {
    parents.resize(_parents.size());
    parentRTTs.resize(_parents.size());
    for (uint32_t p = 0; p < parents.size(); p++) {
        parents[p] = _parents[p];
        parentRTTs[p] = (network)? network->getRTT(name, parents[p]->getName()) : 0;
    }
}


uint64_t DCWSOLIBottomCC::processEviction(Address wbLineAddr, uint32_t lineId, bool lowerLevelWriteback, uint64_t cycle, uint32_t srcId) {
    DCWSOLIState* state = &array[lineId];
    if (lowerLevelWriteback) {
        //If this happens, when tcc issued the invalidations, it got a writeback. This means we have to do a PUTX, i.e. we have to transition to M if we are in E
        assert(*state == D || *state == C || *state == W); //Must have exclusive permission!
        if (*state == D || *state == C){
            *state = D;
        }
//        *state = W; //Silent E->M transition (at eviction); now we'll do a PUTX // We can stay at W
    }
    uint64_t respCycle = cycle;
    switch (*state) {
        case I:
            break; //Nothing to do
        case L:
//            *state = I; //No messages to lower caches, but state needs to be updated.
//            break; // I think Loser needs to send a PUTS messge (which doesn't do anything) in order for the directory to mark that it is no longer a sharer
        case O:
        case S:
        case C:
            {
                MemReq req = {wbLineAddr, PUTS, selfId, state, cycle, &ccLock, *state, srcId, 0 /*no flags*/};
                respCycle = parents[getParentId(wbLineAddr)]->access(req);
            }
            break;
        case W:
        case D:
            {
                MemReq req = {wbLineAddr, PUTX, selfId, state, cycle, &ccLock, *state, srcId, 0 /*no flags*/};
                respCycle = parents[getParentId(wbLineAddr)]->access(req);
            }
            break;

        default: panic("!?");
    }
    assert_msg(*state == I, "Wrong final state %s on eviction", DCWSOLIStateName(*state));
    return respCycle;
}

uint64_t DCWSOLIBottomCC::processAccess(Address lineAddr, uint32_t lineId, AccessType type, uint64_t cycle, uint32_t srcId, uint32_t flags) {
    uint64_t respCycle = cycle;
    DCWSOLIState* state = &array[lineId];
    switch (type) {
        // A PUTS/PUTX does nothing w.r.t. higher coherence levels --- it dies here
        case PUTS: //Clean writeback, nothing to do (except profiling)
            assert(*state != I);
            profPUTS.inc();
            break;
        case PUTX: //Dirty writeback
            assert(*state == D || *state == W || *state == C);//TODO determine if C should exist here
            if (*state == C) {
                //Silent transition, record that block was written to
                *state = D;
            }
            profPUTX.inc();
            break;
        case GETS:
            if (*state == I) {
                uint32_t parentId = getParentId(lineAddr);
                MemReq req = {lineAddr, GETS, selfId, state, cycle, &ccLock, *state, srcId, flags};
                uint32_t nextLevelLat = parents[parentId]->access(req) - cycle;
                uint32_t netLat = parentRTTs[parentId];
                profGETNextLevelLat.inc(nextLevelLat);
                profGETNetLat.inc(netLat);
                respCycle += nextLevelLat + netLat;
                profGETSMiss.inc();
                assert(*state == C || *state == S);
            } else {
                profGETSHit.inc();
            }
            break;
        case GETX:
            if (*state == I || *state == S || *state == O) {
                //Profile before access, state changes
                if (*state == I) profGETXMissIM.inc();
                else profGETXMissSM.inc();
                uint32_t parentId = getParentId(lineAddr);
                MemReq req = {lineAddr, GETX, selfId, state, cycle, &ccLock, *state, srcId, flags};
                uint32_t nextLevelLat = parents[parentId]->access(req) - cycle;
                uint32_t netLat = parentRTTs[parentId];
                profGETNextLevelLat.inc(nextLevelLat);
                profGETNetLat.inc(netLat);
                respCycle += nextLevelLat + netLat;
            } else {
                if (*state == C) {
                    // Silent transition
                    // NOTE: When do we silent-transition E->M on an ML hierarchy... on a GETX, or on a PUTX?
                    /* Actually, on both: on a GETX b/c line's going to be modified anyway, and must do it if it is the L1 (it's OK not
                     * to transition if L2+, we'll TX on the PUTX or invalidate, but doing it this way minimizes the differences between
                     * L1 and L2+ controllers); and on a PUTX, because receiving a PUTX while we're in E indicates the child did a silent
                     * transition and now that it is evictiong, it's our turn to maintain M info.
                     */
                    *state = D;
                }
                profGETXHit.inc();
            }
            assert_msg((*state == D || *state == W || *state == L), "Wrong final state on GETX, lineId %d numLines %d, finalState %s", lineId, numLines, DCWSOLIStateName(*state));
            break;

        default: panic("!?");
    }
    assert_msg(respCycle >= cycle, "XXX %ld %ld", respCycle, cycle);
    return respCycle;
}

void DCWSOLIBottomCC::processWritebackOnAccess(Address lineAddr, uint32_t lineId, AccessType type) {
    DCWSOLIState* state = &array[lineId];
    assert(*state == D || *state == C || *state == W);
    if (*state == C) {
        //Silent transition to M if in E
        *state = D;
    }
}

void DCWSOLIBottomCC::processInval(Address lineAddr, uint32_t lineId, InvType type, bool* reqWriteback) {
    DCWSOLIState* state = &array[lineId];
    assert(*state != I);
    switch (type) {
        case INVX: //lose exclusivity
            //Hmmm, do we have to propagate loss of exclusivity down the tree? (nah, topcc will do this automatically -- it knows the final state, always!)
            assert_msg(*state == D || *state == C, "Invalid state %s", DCWSOLIStateName(*state));
            if (*state == D){
//                *reqWriteback = true; //I don't think this need to happen.
                *state = W;
            }
            if (*state == C){
                *state = S;
                *reqWriteback = true;
            }
            profINVX.inc();
            break;
        case INV: //invalidate
            assert(*state != I);
            if (*state == D || *state == W || *state == C) *reqWriteback = true;
            *state = I;
            profINV.inc();
            break;
        case FWD: //forward
            assert_msg(*state == C || *state == S, "Invalid state %s on FWD", DCWSOLIStateName(*state));
            profFWD.inc();
            break;
        case BAR: //barrier
            switch (*state) {
                case D:
                case C:
                    *state = C;
                    break;
                case W:
                    *reqWriteback = true;
                case S:
                    *state = O;
                    break;
                case O:
                case L:
                case I:
                    *state = I;
                    break;
                default: panic("!?");
            }
            break;
        default: panic("!?");
    }
    //NOTE: BottomCC never calls up on an invalidate, so it adds no extra latency
}


uint64_t DCWSOLIBottomCC::processNonInclusiveWriteback(Address lineAddr, AccessType type, uint64_t cycle, DCWSOLIState* state, uint32_t srcId, uint32_t flags) {
    if (!nonInclusiveHack) panic("Non-inclusive %s on line 0x%lx, this cache should be inclusive", AccessTypeName(type), lineAddr);

    //info("Non-inclusive wback, forwarding");
    MemReq req = {lineAddr, type, selfId, state, cycle, &ccLock, *state, srcId, flags | MemReq::NONINCLWB};
    uint64_t respCycle = parents[getParentId(lineAddr)]->access(req);
    return respCycle;
}


/* MESITopCC implementation */

void DCWSOLITopCC::init(const g_vector<BaseCache*>& _children, Network* network, const char* name) {
    if (_children.size() > MAX_CACHE_CHILDREN) {
        panic("[%s] Children size (%d) > MAX_CACHE_CHILDREN (%d)", name, (uint32_t)_children.size(), MAX_CACHE_CHILDREN);
    }
    children.resize(_children.size());
    childrenRTTs.resize(_children.size());
    for (uint32_t c = 0; c < children.size(); c++) {
        children[c] = _children[c];
        childrenRTTs[c] = (network)? network->getRTT(name, children[c]->getName()) : 0;
    }
}

uint64_t DCWSOLITopCC::sendInvalidates(Address lineAddr, uint32_t lineId, InvType type, bool* reqWriteback, uint64_t cycle, uint32_t srcId) {
    //Send down downgrades/invalidates
    Entry* e = &array[lineId];

    //Don't propagate downgrades if sharers are not exclusive.
    if (type == INVX && !e->isExclusive()) {
        return cycle;
    }

    uint64_t maxCycle = cycle; //keep maximum cycle only, we assume all invals are sent in parallel
    if (!e->isEmpty()) {
        uint32_t numChildren = children.size();
        uint32_t sentInvs = 0;
        for (uint32_t c = 0; c < numChildren; c++) {
            if (e->sharers[c]) {
                InvReq req = {lineAddr, type, reqWriteback, cycle, srcId};
                uint64_t respCycle = children[c]->invalidate(req);
                respCycle += childrenRTTs[c];
                maxCycle = MAX(respCycle, maxCycle);
                if (type == INV) e->sharers[c] = false;
                sentInvs++;
            }
        }
        assert(sentInvs == e->numSharers);
        if (type == INV) {
            e->numSharers = 0;
        } else {
            //TODO: This is kludgy -- once the sharers format is more sophisticated, handle downgrades with a different codepath
            assert(e->exclusive);
            assert(e->numSharers == 1);
            e->exclusive = false;
        }
    }
    return maxCycle;
}


uint64_t DCWSOLITopCC::processEviction(Address wbLineAddr, uint32_t lineId, bool* reqWriteback, uint64_t cycle, uint32_t srcId) {
    if (nonInclusiveHack) {
        // Don't invalidate anything, just clear our entry
        array[lineId].clear();
        return cycle;
    } else {
        //Send down invalidates
        return sendInvalidates(wbLineAddr, lineId, INV, reqWriteback, cycle, srcId);
    }
}

uint64_t DCWSOLITopCC::processAccess(Address lineAddr, uint32_t lineId, AccessType type, uint32_t childId, bool haveExclusive,
                                  DCWSOLIState* childState, bool* inducedWriteback, uint64_t cycle, uint32_t srcId, uint32_t flags) {
    Entry* e = &array[lineId];
    uint64_t respCycle = cycle;
    switch (type) {
        case PUTX:
            assert(e->isExclusive());
            if (flags & MemReq::PUTX_KEEPEXCL) {
                assert(e->sharers[childId]);
                assert(*childState == D);// || *childState == W);
//                *childState = E; //they don't hold dirty data anymore
//                if (*childState == D){
                    *childState = C;
//                }else if (*childState == W){
//                    *childState = S;
//                }

                break; //don't remove from sharer set. It'll keep exclusive perms.
            }
            //note NO break in general
        case PUTS:
            assert(e->sharers[childId]);
            e->sharers[childId] = false;
            e->numSharers--;
            *childState = I;
            break;
        case GETS:
            if (e->isEmpty() && haveExclusive && !(flags & MemReq::NOEXCL)) {
                //Give in E state
                e->exclusive = true;
                e->sharers[childId] = true;
                e->numSharers = 1;
                *childState = C;
            } else {
                //Give in S state
                assert(e->sharers[childId] == false);

                if (e->isExclusive()) {
                    //Downgrade the exclusive sharer
                    respCycle = sendInvalidates(lineAddr, lineId, INVX, inducedWriteback, cycle, srcId);
                }

                assert_msg(!e->isExclusive(), "Can't have exclusivity here. isExcl=%d excl=%d numSharers=%d", e->isExclusive(), e->exclusive, e->numSharers);

                e->sharers[childId] = true;
                e->numSharers++;
                e->exclusive = false; //dsm: Must set, we're explicitly non-exclusive
                *childState = S;
            }
            break;
        case GETX:
//TODO This case handles winning and losing. I need to actually write it.
            assert(haveExclusive); //the current cache better have exclusive access to this line

            if (e->exclusive == true){
                if (e->winner != childId){
                    *childState = L;
                    e->sharers[childId] = true;
                    e->numSharers++;
                    if (e->numSharers == 2){
                        //TODO send contention message to the child - I think I have modified sendInvalidates to do this
                        respCycle = sendInvalidates(lineAddr, lineId, INVX, inducedWriteback, cycle, srcId);
                    }
                }
            }else{
                // If child is in sharers list (this is an upgrade miss), take it out
                if (e->sharers[childId]) {
                    assert_msg(!e->isExclusive(), "Spurious GETX, childId=%d numSharers=%d isExcl=%d excl=%d", childId, e->numSharers, e->isExclusive(), e->exclusive);
                    e->sharers[childId] = false;
                    e->numSharers--;
                }
    
                // Invalidate all other copies
                respCycle = sendInvalidates(lineAddr, lineId, INV, inducedWriteback, cycle, srcId);
    
                // Set current sharer, mark exclusive
                e->sharers[childId] = true;
                e->numSharers++;
                e->exclusive = true;
    
                assert(e->numSharers == 1);
    
                *childState = D; //give in M directly
                    
            }

            break;

        default: panic("!?");
    }

// Check code added to help debug, should be removed
    uint32_t temp = e->numSharers;
    for (uint32_t c=0; c<MAX_CACHE_CHILDREN; ++c){
        if (e->sharers[c]){
            --temp;
        }
    }
    if (temp > 0){
        unsigned int marker = 0;
        switch (type){
            case PUTX:
                marker = 1;
                break;
            case PUTS:
                marker = 2;
                break;
            case GETX:
                marker = 3;
                break;
            case GETS:
                marker = 4;
                break;
        }
        panic("sharer tracking failed in processAccess, marker = %u", marker);
    }

    return respCycle;
}

uint64_t DCWSOLITopCC::processInval(Address lineAddr, uint32_t lineId, InvType type, bool* reqWriteback, uint64_t cycle, uint32_t srcId) {
    if (type == FWD) {//if it's a FWD, we should be inclusive for now, so we must have the line, just invLat works
        assert(!nonInclusiveHack); //dsm: ask me if you see this failing and don't know why
        return cycle;
    } else {
        //Just invalidate or downgrade down to children as needed
        return sendInvalidates(lineAddr, lineId, type, reqWriteback, cycle, srcId);
    }
}

