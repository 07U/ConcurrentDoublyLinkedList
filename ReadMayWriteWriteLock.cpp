/*==============================================================================
 *******************************************************************************
 *==============================================================================
 * File name: ReadMayWriteWriteLock.cpp
 * Author:    Oz Davidi
 *
 *                     ****     *********  ***   ***
 *                    ******    ********   ***   ***
 *                   ***  ***       ***    ***   ***
 *                   ***  ***      ***     ***   ***
 *                   ***  ***     ***      ***   ***
 *                    ******     ***        *******
 *                     ****     ***          *****
 *
 *==============================================================================
 *******************************************************************************
 *============================================================================*/

/**=============================================================================
 * Includes:
 * ===========================================================================*/

#include "ReadMayWriteWriteLock.h"
#include <cassert>

using std::scoped_lock;
using std::make_tuple;
using std::make_shared;
using std::get;

/**=============================================================================
 * Declarations:
 * ===========================================================================*/

typedef Read_MayWrite_Write_Lock Lock;

/*==============================================================================
 * Implementation:
 *============================================================================*/

/*******************************************************************************
 * Read_MayWrite_Write_Lock:
 ******************************************************************************/

/* private:
 **********/

bool Lock::CanReaderAcquireLock() const noexcept{
    return !isWriterHolding;
}

bool Lock::CanMayWriterAcquireLock() const noexcept {
    return mayWriterThreadId == thread::id() && CanReaderAcquireLock();
}

bool Lock::CanWriterAcquireLock() const noexcept {
    return readersNumber == 0 && CanMayWriterAcquireLock();
}

bool Lock::CanAcquireLock(const Operation operation) const noexcept {
    bool canAcquireLock(true);
    switch(operation) {
        case READ:
            canAcquireLock = CanReaderAcquireLock();
            break;
        case MAY_WRITE:
            canAcquireLock = CanMayWriterAcquireLock();
            break;
        case WRITE:
            canAcquireLock = CanWriterAcquireLock();
            break;
        default:
            // We compile with -Wswitch-default and -Werror.
            // This case should never happen. Let us introduce an assert anyway.
            assert(operation != READ      && \
                   operation != MAY_WRITE && \
                   operation != WRITE);
    }
    return canAcquireLock;
}

void Lock::InsertConditionTuple(const Operation operation,
                                const bool isVIP/* = false*/) {
    ConditionTuple conditionTuple(make_tuple(make_shared<condition_variable>(),
                                             operation,
                                             1));
    if(isVIP) {
        threadQueue.push_front(std::move(conditionTuple));
    } else {
        threadQueue.push_back(std::move(conditionTuple));
    }
}

bool Lock::ShouldThreadWait(const Operation operation) {
    bool shouldThreadWait(true); // This is important for the default case!

    switch(threadQueue.size()) {
        case 0:
            shouldThreadWait = !CanAcquireLock(operation);
            break;
        case 1:
            shouldThreadWait = operation != READ                   || \
                               get<1>(threadQueue.front()) != READ || \
                               !CanAcquireLock(operation);
            [[fallthrough]];
        default:
            if(operation == READ && shouldThreadWait) {
                ConditionTuple& backTuple(threadQueue.back());
                if(get<1>(backTuple) == READ) {
                    unsigned int& readTupleCounter(get<2>(backTuple));
                    assert(readTupleCounter > 0);
                    ++readTupleCounter;
                    return shouldThreadWait; // This is an edge case in which we
                                             // do not want to update
                                             // shouldThreadWait, but just get
                                             // out, returning a true value. We
                                             // evade a queue insertion, as a
                                             // condition variable was allready
                                             // inserted.
                }
            }
    }

    if(shouldThreadWait) InsertConditionTuple(operation);

    return shouldThreadWait;
}

void Lock::Wait(const Operation operation, unique_lock<mutex>& lock) noexcept {
    if(!ShouldThreadWait(operation)) return;

    const ConditionPtr& conditionPtr(get<0>(threadQueue.back()));
    do {
        conditionPtr->wait(lock);
    // In an ideal world, we could have avoided this check, as we manage a queue
    // and control who is being notified and when. But, in our world, there are
    // SPURIOUS WAKEUPS, which can awaken threads even when their condition
    // variable was not signaled. :/
    } while(get<0>(threadQueue.front()) != conditionPtr || \
            !CanAcquireLock(operation));
    
    if(operation == READ) {
        ConditionTuple& readTuple(threadQueue.front());
        unsigned int& readTupleCounter(get<2>(readTuple));
        assert(readTupleCounter > 0);
        --readTupleCounter;

        if(readTupleCounter != 0) return;
    }

    threadQueue.pop_front();

    // A philanthropic piece of code. Readers an may-writers take care of each
    // other. We prefer to check the condition instead of awakening a thread
    // and let it check the condition by itself just to return to waiting.
    if(operation != WRITE) TryNotifyingNext();
}

void Lock::TryNotifyingNext() const noexcept {
    if(!threadQueue.empty()) {
        const ConditionTuple& frontTuple(threadQueue.front());
        const Operation operation(get<1>(frontTuple));
        if(CanAcquireLock(operation)) {
            get<0>(frontTuple)->notify_all();
        }
    }
}

/* public:
 *********/

Lock::Read_MayWrite_Write_Lock() : readersNumber(0), isWriterHolding(false) {
}

void Lock::LockRead() {
    unique_lock<mutex> lock(internalMutex);

    Wait(READ, lock);

    ++readersNumber;
}

void Lock::LockMayWrite() {
    unique_lock<mutex> lock(internalMutex);

    Wait(MAY_WRITE, lock);

    ++readersNumber;
    assert(mayWriterThreadId == thread::id());
    mayWriterThreadId = std::this_thread::get_id();
}

void Lock::LockWrite() {
    unique_lock<mutex> lock(internalMutex);

    Wait(WRITE, lock);

    assert(!isWriterHolding);
    isWriterHolding = true;
}

void Lock::UpgradeLock() {
    unique_lock<mutex> lock(internalMutex);
    
    assert(readersNumber > 0);
    --readersNumber;
    assert(mayWriterThreadId == std::this_thread::get_id());
    mayWriterThreadId = thread::id();
    
    if(!CanWriterAcquireLock()) {
        InsertConditionTuple(WRITE, /*isVIP = */true);
        
        const ConditionPtr& conditionPtr(get<0>(threadQueue.front()));
        do {
            conditionPtr->wait(lock);
        // In an ideal world, we could have avoided this check, as we manage a
        // queue and control who is being notified and when. But, in our world,
        // there are SPURIOUS WAKEUPS, which can awaken threads even when their
        // condition variable was not signaled. :/
        } while(!CanWriterAcquireLock());

        threadQueue.pop_front();
    }

    assert(!isWriterHolding);
    isWriterHolding = true;
}

void Lock::ReleaseSharedLock() noexcept {
    scoped_lock<mutex> lock(internalMutex);

    assert(readersNumber > 0);
    --readersNumber;

    if(mayWriterThreadId == std::this_thread::get_id()) {
        mayWriterThreadId = thread::id();
    } else {
        if(readersNumber > 0) return;
    }

    TryNotifyingNext(); // Only* a may-writer or a writer can be notified here.
                        // The TryNotifyingNext() method must be used, because
                        // there may be a scenario when a may-writer just
                        // released his lock, but next thread in-line is a
                        // writer. A check for this must be made before waking
                        // the writer.
                        //
                        // * A possible case that can occur is a double
                        //   notification by a may-writer of already awoken
                        //   readers.
}

void Lock::ReleaseExclusiveLock() noexcept {
    scoped_lock<mutex> lock(internalMutex);

    assert(isWriterHolding);
    isWriterHolding = false;

    // No use of TryNotifyingNext() because no need for a check. Any thread next
    // in-line can enter.
    if(!threadQueue.empty()) {
        get<0>(threadQueue.front())->notify_all();
    }
}

/**=============================================================================
 * End of file
 * ===========================================================================*/