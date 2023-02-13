/**
 * Copyright (c) 2022 Ashish Kumar Rambhatla
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Definitions of a LRU-IPV replacement policy.
 */

#include "mem/cache/replacement_policies/lru_ipv.hh"

#include <cmath>
#include <numeric>

#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/LruIpv.hh"

#define INVALID_RECENCY_VALUE 16


namespace ReplacementPolicy {

/* Constructor for the Replacement data struct. */
LRUIPVRP::LRUIPVReplData::LRUIPVReplData(const uint64_t set_id, const uint64_t index, std::shared_ptr<RecencyStack> stack)
  : set_id(set_id), index(index), stack(stack) {}

/* Constructor for the Replacement policy class. */
LRUIPVRP::LRUIPVRP(const Params &p) 
    :   Base(p), 
        numWays(p.numWays),
        blockInstanceCounter(0),
        tempStack(nullptr)
{
    DPRINTF(LruIpv,
            "Number of ways must be non-zero and a power of 2. It is %d\n", !isPowerOf2(numWays));
    promotionVector = {0,0,1,0,3,0,1,2,1,0,5,1,0,0,1,11,13}; // copy the promo vector from paper
}

/**
 * @brief InstantiateEntry: Intializes the replacement data struct for each cache set. It associates
 *         the shared replacement data structure of the set to all the blocks within a cache set.
 * 
 * @return std::shared_ptr<ReplacementData> 
 */
std::shared_ptr<ReplacementData> LRUIPVRP::instantiateEntry()
{
    // Generate a shared recency stack per set.
    if (blockInstanceCounter % numWays == 0) {
        tempStack = std::make_shared<RecencyStack>(numWays, 0);
        std::iota(tempStack->begin(), tempStack->end(), 0);
    }

    uint64_t set_id = (uint64_t)(blockInstanceCounter / numWays); 
    uint64_t index = blockInstanceCounter % numWays;
    auto ipvReplData = std::make_shared<LRUIPVReplData>(set_id, index, tempStack);

    // Update instance blockInstanceCounter
    blockInstanceCounter++;

    return ipvReplData;
}

/**
 * @brief printSharedState: Helper function to print the recency stack of a set.
 * 
 * @param replacement_data 
 */
void LRUIPVRP::printSharedState(const std::shared_ptr<ReplacementData>& replacement_data) const {
    
    std::shared_ptr<LRUIPVReplData> lru_ipv_replacement_data =
        std::static_pointer_cast<LRUIPVReplData>(replacement_data);
    auto stack_ptr = lru_ipv_replacement_data->stack;   

    for (auto iter = stack_ptr->begin(); iter != stack_ptr->end(); iter++) {
        DPRINTF(LruIpv, "%d ", *iter);
    }  
        DPRINTF(LruIpv,"\n");
}

/**
 * @brief invalidate: Entry point to invalidate a specific block within a set.
 *                    This is achieved by moving the recency value of that 
 *                    block to the MRU position.
 * 
 * @param replacement_data 
 */
void LRUIPVRP::invalidate(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Cast replacement data
    std::shared_ptr<LRUIPVReplData> lru_ipv_replacement_data =
        std::static_pointer_cast<LRUIPVReplData>(replacement_data);
    auto stack_ptr = lru_ipv_replacement_data->stack;
    uint64_t block_index = lru_ipv_replacement_data->index;
    uint64_t set_id = lru_ipv_replacement_data->set_id;
    
    uint64_t target_stack_val = stack_ptr->at(block_index);

    if (target_stack_val >= numWays) {
        target_stack_val = numWays - 1;
    }
    DPRINTF(LruIpv,"\ninvalidate:  replacement data index : %d\n", block_index);
    int i = numWays;
    uint64_t new_stack_val = numWays;
    DPRINTF(LruIpv,"\ninvalidate: set_id: %d\n target_stack_val : %d\n",set_id, target_stack_val);
    DPRINTF(LruIpv,"invalidate: Before modification : \n");
    // increase the recency value to invalid, i.e 16 in this case.
    do {
        i--;
        uint64_t currBlock_stack_val = stack_ptr->at(i);
        if (currBlock_stack_val == target_stack_val) {
            stack_ptr->at(i) = new_stack_val;
        } else if (currBlock_stack_val > target_stack_val && currBlock_stack_val <= new_stack_val) {
            stack_ptr->at(i) = currBlock_stack_val - 1;
        }
    } while ( i > 0);
    DPRINTF(LruIpv,"invalidate: After modification : \n");
    printSharedState(replacement_data);
}

/**
 * @brief touch: Entry function to promote a block to its new position
 *               when a cache hit happens.
 * 
 * @param replacement_data 
 */
void LRUIPVRP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Cast replacement data
    std::shared_ptr<LRUIPVReplData> lru_ipv_replacement_data =
        std::static_pointer_cast<LRUIPVReplData>(replacement_data);
    auto stack_ptr = lru_ipv_replacement_data->stack;
    uint64_t block_index = lru_ipv_replacement_data->index;
    uint64_t set_id = lru_ipv_replacement_data->set_id;
    uint64_t target_stack_val = stack_ptr->at(block_index);

    if (target_stack_val >= numWays) {
        target_stack_val = numWays - 1;
    }

    uint64_t new_stack_val = promotionVector[target_stack_val];
    DPRINTF(LruIpv,"\ntouch new_stack_val : %d, old_stack_val: %d\n",new_stack_val, target_stack_val);
    DPRINTF(LruIpv,"touch: Before modification : \n");
    printSharedState(replacement_data);
    DPRINTF(LruIpv,"\ntouch: set_id:%d target_stack_val : %d numWays : %d\n",set_id, target_stack_val, numWays);
    int i = numWays;
    // Promoting the block's recency value to a new position.
    do {
        i--;
        uint64_t currBlock_stack_val = stack_ptr->at(i);
        if (currBlock_stack_val >= numWays) {
            currBlock_stack_val = numWays - 1;
        }
        if (currBlock_stack_val == target_stack_val) {
            stack_ptr->at(i) = new_stack_val;
        } else if (currBlock_stack_val >= new_stack_val && currBlock_stack_val < target_stack_val) {
            stack_ptr->at(i) = ++currBlock_stack_val;
        }
    } while ( i > 0);
    DPRINTF(LruIpv,"touch: After modification : \n");
    printSharedState(replacement_data);
    DPRINTF(LruIpv,"\n");
}

/**
 * @brief reset: Entry point for resetting a block's recency value.
 * 
 * @param replacement_data 
 */
void LRUIPVRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<LRUIPVReplData> lru_ipv_replacement_data =
        std::static_pointer_cast<LRUIPVReplData>(replacement_data);
    auto stack_ptr = lru_ipv_replacement_data->stack;
    uint64_t block_index = lru_ipv_replacement_data->index;
    uint64_t set_id = lru_ipv_replacement_data->set_id;
    uint64_t target_stack_val = stack_ptr->at(block_index);
    if (target_stack_val >= numWays) {
        target_stack_val = numWays - 1;
    }

    uint64_t new_stack_val = promotionVector[16];
    DPRINTF(LruIpv,"\nreset: new_stack_val : %d\n",new_stack_val);
    DPRINTF(LruIpv,"\nreset: target_stack_val : %d\n",target_stack_val);
    DPRINTF(LruIpv,"reset: Before modification : \n");
    printSharedState(replacement_data);
    DPRINTF(LruIpv,"\nreset: target_stack_val : %d numWays : %d\n",target_stack_val, numWays);
    int i = numWays;
    // Restting the recency value to a new block position.
    do {
        i--;
        uint64_t currBlock_stack_val = stack_ptr->at(i);
        if (currBlock_stack_val >= numWays) {
            currBlock_stack_val = numWays - 1;
        }
        if (currBlock_stack_val == target_stack_val) {
            stack_ptr->at(i) = new_stack_val;
            DPRINTF(LruIpv,"\reset: set_id:%d tagert_stack_val : %d temp_index : %d\n",set_id, target_stack_val,currBlock_stack_val);
        } else if (currBlock_stack_val >= new_stack_val && currBlock_stack_val < target_stack_val) {
            stack_ptr->at(i) = ++currBlock_stack_val;
        }
    } while ( i > 0);
    DPRINTF(LruIpv,"reset: After modification : \n");
    printSharedState(replacement_data);
    DPRINTF(LruIpv,"\n");
}

/**
 * @brief getVictim: Entry point for finding a victim block to be evicted.
 * 
 * @param candidates : List of viable candidates for eviction with in a set.
 * @return ReplaceableEntry* 
 */
ReplaceableEntry* LRUIPVRP::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    ReplaceableEntry* victim = candidates[0];
    // Iterate through all candidates and checking which one is at MRU position to get evicted.
    for (const auto &candidate: candidates) {
        auto candidate_repl_data = std::static_pointer_cast<LRUIPVReplData>(candidate->replacementData);
        uint64_t candidate_index = candidate_repl_data->index;
        auto candidate_stack_ptr = candidate_repl_data->stack;
        uint64_t candidate_stack_value = candidate_stack_ptr->at(candidate_index);
        if (candidate_stack_value >= (numWays-1)) {
            victim = candidate;
            DPRINTF(LruIpv, "In getVictim. SetID: %d\n", candidate_repl_data->set_id);
            DPRINTF(LruIpv,"\ngetVictim: victim_index : %d,victim_stack_value : %d\n", candidate_index, candidate_stack_value);
        }
        DPRINTF(LruIpv,"\ngetVictim: candidate_index : %d, stack_size : %d\n",candidate_index, candidate_stack_ptr->size());
    }

    return victim;
}

} // namespace ReplacementPolicy