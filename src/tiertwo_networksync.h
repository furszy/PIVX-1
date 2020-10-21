// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef TIERTWO_NETWORKSYNC_H
#define TIERTWO_NETWORKSYNC_H

#include "net.h" // for NodeId

#include <map>

class CMasternodeSync;

struct TierTwoPeerData {
    // map of message --> last request timestamp, bool hasResponseArrived.
    std::map<const char*, std::pair<int64_t, bool>> mapMsgData;
};

class ProtectedVector
{
private:
    std::vector<uint256> vector;
    Mutex cs;

    bool has(const uint256& hash) {
        return std::any_of(vector.begin(), vector.end(), [hash](const uint256& _hash){ return hash == _hash; } );
    }

public:
    // returns false if the item is already on the vector
    bool tryAppendItem(const uint256& hash) {
        LOCK(cs);
        if (has(hash)) return false;
        vector.emplace_back(hash);
        return true;
    }
    bool contains(const uint256& hash) {
        return WITH_LOCK(cs, return has(hash););
    }
};

// Class in charge of managing the tier two synchronization.
class TierTwoSyncMan
{
private:
    // Old sync class, needed for now.
    CMasternodeSync* syncParent{nullptr};

    // Sync node state
    // map of nodeID --> TierTwoPeerData
    std::map<NodeId, TierTwoPeerData> peersSyncState;

    template <typename... Args>
    void RequestDataTo(CNode* pnode, const char* msg, bool forceRequest, Args&&... args);

    template <typename... Args>
    void PushMessage(CNode* pnode, const char* msg, Args&&... args);

    // update peer sync state data
    bool UpdatePeerSyncState(const NodeId& id, const char* msg, const int nextSyncStatus);

    // Check if an update is needed
    void CheckAndUpdateSyncStatus();

    ProtectedVector* GetSeenItemsVector(int type);

    // DoS spam filtering protection, guarded by its own internal mutex.
    // For now, it only protects us from seen proposals and budgets.
    ProtectedVector seenBudgetItems;
    ProtectedVector seenProposalsItems;

public:

    explicit TierTwoSyncMan(CMasternodeSync* _syncParent) : syncParent(_syncParent) {}

    // Sync message dispatcher
    bool MessageDispatcher(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void SyncRegtest(CNode* pnode);

    // Check if we already have seen the item
    bool AlreadyHave(const uint256& hash, int type);
    bool TryAppendItem(const uint256& hash, int type);

};

#endif //TIERTWO_NETWORKSYNC_H
