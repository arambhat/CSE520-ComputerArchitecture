/*
 * Copyright (c) 2014 The Regents of The University of Michigan
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

/* @file
 * Implementation of a gselect branch predictor
 */

#include "cpu/pred/gselect.hh"

#include "base/intmath.hh"
#include "base/bitfield.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/Fetch.hh"
#include "debug/Mispredict.hh"
#include "debug/GSDebug.hh"


GSelectBP::GSelectBP(const GSelectBPParams &params)
    : BPredUnit(params),
      globalHistoryReg(params.numThreads, 0),
      globalHistoryBits(params.globalHistoryBits),
      phtCtrBits(params.PHTCtrBits),
      predictorSize(params.PredictorSize),
      finalCounters(predictorSize, SatCounter8(phtCtrBits))
{
    if(!isPowerOf2(predictorSize)) {
        fatal("Invalid predictor size.\n");
    }
    globalHistoryMask = mask(globalHistoryBits);
    DPRINTF(GSDebug, "The global history mask is: %d\n", globalHistoryMask);
    branchAddressBits = ceilLog2(predictorSize) - globalHistoryBits;
    branchAddressMask = mask(branchAddressBits);
    predictionThreshold = (ULL(1) << (phtCtrBits - 1)) - 1;
}

void GSelectBP::uncondBranch(ThreadID tid, Addr pc, void * &bp_history)
{
    BPHistory *history = new BPHistory;
    history->globalHistoryReg = globalHistoryReg[tid] & globalHistoryMask;
    history->finalPred = true;
    bp_history = static_cast<void*>(history);
    DPRINTF(GSDebug, "In uncondBranch. Global history register is: %d. Branch address = %d\n", globalHistoryReg[tid], pc);
    updateGlobalHistReg(tid, true);
}

void GSelectBP::squash(ThreadID tid, void *bp_history)
{
    DPRINTF(GSDebug, "In squash. Global history register is (initially): %0x.\n", globalHistoryReg[tid]);
    BPHistory *history = static_cast<BPHistory*>(bp_history);
    globalHistoryReg[tid] = history->globalHistoryReg & globalHistoryMask;
    DPRINTF(GSDebug, "In squash. Global history register is (finally): %0x.\n", globalHistoryReg[tid]);
    delete history;
}

/*
 * Here we lookup the actual branch prediction. We use the PC to
 * identify the bias of a particular branch, which is based on the
 * prediction in the choice array. A hash of the global history
 * register and a branch's PC is used to index into both the taken
 * and not-taken predictors, which both present a prediction. The
 * choice array's prediction is used to select between the two
 * direction predictors for the final branch prediction.
 */
bool
GSelectBP::lookup(ThreadID tid, Addr branch_addr, void * &bp_history)
{
    DPRINTF(GSDebug, "In lookup. Globalbranch address = %d\n, branchAddr: %d, ", branch_addr);
    unsigned branchAddressIdx = ((branch_addr >> instShiftAmt) & branchAddressMask);
    unsigned globalHistoryIdx = ((globalHistoryReg[tid]) & globalHistoryMask);
    unsigned finalIdx =  (((globalHistoryIdx << branchAddressBits) | branchAddressIdx)) & mask(ceilLog2(predictorSize));

    BPHistory *history = new BPHistory;

    assert(finalIdx < predictorSize);
    bool prediction = finalCounters[finalIdx] > predictionThreshold;


    history->globalHistoryReg = globalHistoryReg[tid] & globalHistoryMask;
    history->finalPred = prediction;
    bp_history = static_cast<void*>(history);
    updateGlobalHistReg(tid, prediction);
    return prediction;
}

void GSelectBP::btbUpdate(ThreadID tid, Addr branch_addr, void * &bp_history)
{
    DPRINTF(GSDebug,"BTBUPDATE FUNCTION , globalHistoryReg Before mod: %0x\n",globalHistoryReg[tid]);
    globalHistoryReg[tid] &= (globalHistoryMask & ~ULL(1));
    DPRINTF(GSDebug,"BTBUPDATE FUNCTION , globalHistoryReg After mod: %0x\n",globalHistoryReg[tid]);
}

void GSelectBP::update(ThreadID tid, Addr branch_addr, bool taken, void *bp_history,
            bool squashed, const StaticInstPtr & inst, Addr corrTarget)
{
    DPRINTF(GSDebug, "In update. branch address = %d\n", branch_addr);
    assert(bp_history);
    BPHistory *history = static_cast<BPHistory*>(bp_history);
    if (squashed) {
        globalHistoryReg[tid] = ((history->globalHistoryReg << 1) | taken) & globalHistoryMask;
    	DPRINTF(GSDebug,"SQUASHED : UPDATE FUNCTION ENDS, HISTORY REG : %x\n",globalHistoryReg[tid]);
        return;
    }
    unsigned branchAddressIdx = ((branch_addr >> instShiftAmt) & branchAddressMask);
    unsigned globalHistoryIdx = (history->globalHistoryReg & globalHistoryMask);
    unsigned finalIdx =  (((globalHistoryIdx << branchAddressBits) | branchAddressIdx)) & mask(ceilLog2(predictorSize));
    assert(finalIdx < predictorSize);

    if (taken) {
        finalCounters[finalIdx]++;
    } else {
        finalCounters[finalIdx]--;
    }
    delete history;
}

void GSelectBP::updateGlobalHistReg(ThreadID tid, bool taken)
{
    globalHistoryReg[tid] = taken ? (globalHistoryReg[tid] << 1) | 1 :
                               (globalHistoryReg[tid] << 1);
    globalHistoryReg[tid] &= globalHistoryMask;
}