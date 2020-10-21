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

public:

    explicit TierTwoSyncMan(CMasternodeSync* _syncParent) : syncParent(_syncParent) {}

    // Sync message dispatcher
    bool MessageDispatcher(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void SyncRegtest(CNode* pnode);
};

#endif //TIERTWO_NETWORKSYNC_H
