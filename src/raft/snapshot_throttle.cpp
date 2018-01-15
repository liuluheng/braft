// libraft - Quorum-based replication of states across machines.
// Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved

// Author: Xiong Kai (xiongkai@baidu.com)
// Date: 2017/08/25 16:15:24

#include <base/time.h>
#include <gflags/gflags.h>
#include <baidu/rpc/reloadable_flags.h>
#include "raft/snapshot_throttle.h"
#include "raft/util.h"

namespace raft {

ThroughputSnapshotThrottle::ThroughputSnapshotThrottle(
        int64_t throttle_throughput_bytes, int64_t check_cycle) 
    : _throttle_throughput_bytes(throttle_throughput_bytes)
    , _check_cycle(check_cycle)
    , _last_throughput_check_time_us(
            caculate_check_time_us(base::cpuwide_time_us(), check_cycle))
    , _cur_throughput_bytes(0)
{}

ThroughputSnapshotThrottle::~ThroughputSnapshotThrottle() {}

size_t ThroughputSnapshotThrottle::throttled_by_throughput(int64_t bytes) {
    size_t available_size = bytes;
    int64_t now = base::cpuwide_time_us();
    int64_t limit_per_cycle = _throttle_throughput_bytes / _check_cycle;
    std::unique_lock<raft_mutex_t> lck(_mutex);
    if (_cur_throughput_bytes + bytes > limit_per_cycle) {
        // reading another |bytes| excceds the limit
        if (now - _last_throughput_check_time_us <= 
            1 * 1000 * 1000 / _check_cycle) {
            // if time interval is less than or equal to a cycle, read more data
            // to make full use of the throughput of current cycle.
            available_size = limit_per_cycle - _cur_throughput_bytes;
            _cur_throughput_bytes = limit_per_cycle;
        } else {
            // otherwise, read the data in the next cycle.
            available_size = bytes > limit_per_cycle ? limit_per_cycle : bytes;
            _cur_throughput_bytes = available_size;
            _last_throughput_check_time_us = 
                caculate_check_time_us(now, _check_cycle);
        }
    } else {
        // reading another |bytes| doesn't excced limit(less than or equal to), 
        // put it in current cycle
        available_size = bytes;
        _cur_throughput_bytes += available_size;
    }
    lck.unlock();
    return available_size;
}

}  // namespace raft

