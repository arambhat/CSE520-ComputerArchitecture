#ifndef __CPU_PRED_GSELECT_PRED_HH__
#define __CPU_PRED_GSELECT_PRED_HH__

#include <vector>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "cpu/pred/bpred_unit.hh"
#include "base/sat_counter.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/GSelectBP.hh"


class GSelectBP : public BPredUnit
{
    public:
        GSelectBP(const GSelectBPParams &params);
        void uncondBranch(ThreadID tid, Addr pc, void * &bp_history);
        void squash(ThreadID tid, void *bp_history);
        bool lookup(ThreadID tid, Addr branch_addr, void * &bp_history);
        void btbUpdate(ThreadID tid, Addr branch_addr, void * &bp_history);
        void update(ThreadID tid, Addr branch_addr, bool taken, void *bp_history,
                    bool squashed, const StaticInstPtr & inst, Addr corrTarget);
    private:
        void updateGlobalHistReg(ThreadID tid, bool taken);

        struct BPHistory {
            unsigned globalHistoryReg;
            bool finalPred;
        };

        unsigned getGlobalIndex(ThreadID tid,Addr branchAddr,unsigned historyReg);
        std::vector<unsigned> globalHistoryReg;
        unsigned globalHistoryBits;
        unsigned globalHistoryMask;
        unsigned branchAddressBits;
        unsigned phtCtrBits;
        unsigned predictorSize;
        unsigned branchAddressMask;
        std::vector<SatCounter8> finalCounters;

        unsigned predictionThreshold;
};

#endif // __CPU_PRED_GSELECT_PRED_HH__

