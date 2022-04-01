/*==============================================================================
 *******************************************************************************
 *==============================================================================
 * File name: ConcurrentDoublyLinkedList.cpp
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

#include "ConcurrentDoublyLinkedList.h"

using std::make_shared;

/**=============================================================================
 * Declarations:
 * ===========================================================================*/

typedef ConcurrentDoublyLinkedList List;

/*==============================================================================
 * Implementation:
 *============================================================================*/

/*******************************************************************************
 * ConcurrentDoublyLinkedList::Node:
 ******************************************************************************/

/* public:
 *********/

List::Node::Node(const int in_key,
                 const char in_data,
                 const NodePtr& in_prevPtr/* = nullptr*/,
                 const NodePtr& in_nextPtr/* = nullptr*/) : key(in_key),
                                                            data(in_data),
                                                            prevPtr(in_prevPtr),
                                                            nextPtr(in_nextPtr),
                                                            isNodeActive(true) {
}

/*******************************************************************************
 * ConcurrentDoublyLinkedList:
 ******************************************************************************/

/* private:
 **********/

void List::AdvanceAndLockReadMayWrite(NodePtr& prev,
                                      NodePtr& next,
                                      const bool isRead) const noexcept {
    if(isRead) {
        prev = next;
        next = prev->nextPtr;
        prev->lock.ReleaseSharedLock();
        next->lock.LockRead();
    } else {
        prev->lock.ReleaseSharedLock();
        prev = next;
        next = prev->nextPtr;
        next->lock.LockMayWrite();
    }
}

List::NodePtr List::FindKey(NodePtr& position,
                            const int key,
                            const bool isRead/* = false*/) const noexcept {
    NodePtr prev(position), next(isRead ? position : position->nextPtr);
    if(!isRead) {
        next->lock.LockMayWrite();
    }

    while((next->key < key && next != tail) || next == head) {
        AdvanceAndLockReadMayWrite(prev, next, isRead);
    }

    position = prev;
    return next;
}

bool List::InsertFromPosition(const NodePtr& position,
                              const int key,
                              const char data) {
    NodePtr prev(position);
    NodePtr next(FindKey(prev, key));

    bool result(next->key != key || next == tail);
    if(result) {
        prev->lock.UpgradeLock();
        next->lock.UpgradeLock();

        prev->nextPtr = make_shared<Node>(key, data, prev, next);
        next->prevPtr = prev->nextPtr;

        prev->lock.ReleaseExclusiveLock();
        next->lock.ReleaseExclusiveLock();
    } else {
        prev->lock.ReleaseSharedLock();
        next->lock.ReleaseSharedLock();
    }

    return result;
}

/* public:
 *********/

List::ConcurrentDoublyLinkedList() : head(make_shared<Node>(0, '0')),
                                     tail(make_shared<Node>(0, '0')) {
    head->nextPtr = tail;
    tail->prevPtr = head;
}

List::~ConcurrentDoublyLinkedList() {
    for(NodePtr node(head->nextPtr); node != tail; node = node->nextPtr) {
        node->prevPtr->nextPtr = nullptr;
        node->prevPtr = nullptr;
    }
    tail->prevPtr = nullptr;
}

bool List::InsertHead(const int key, const char data) {
    head->lock.LockMayWrite();

    return InsertFromPosition(head, key, data);
}

bool List::InsertTail(const int key, const char data) {
    NodePtr next(tail);
    next->lock.LockRead();
    NodePtr prev(next->prevPtr);
    next->lock.ReleaseSharedLock(); // Not holding any lock now. Mandatory, if
                                    // we don't want to be deadlocked.
    prev->lock.LockMayWrite();

    while((prev->key > key && prev != head) || !prev->isNodeActive) {
        next = prev;
        prev = next->prevPtr;
        next->lock.ReleaseSharedLock(); // Not holding any lock now. Mandatory,
                                        // if we don't want to be deadlocked.
        prev->lock.LockMayWrite();
    }

    if(prev != head && prev->key == key){
        prev->lock.ReleaseSharedLock();
        return false;
    }

    return InsertFromPosition(prev, key, data);
}

bool List::Delete(const int key) noexcept {
    NodePtr prev(head);
    prev->lock.LockMayWrite();
    NodePtr next(FindKey(prev, key));

    bool result(next->key == key && next != tail);
    if(result) {
        prev->lock.UpgradeLock();
        next->lock.UpgradeLock();

        NodePtr del(next);
        next = del->nextPtr;
        next->lock.LockWrite();

        prev->nextPtr = next;
        next->prevPtr = prev;
        del->isNodeActive = false;

        prev->lock.ReleaseExclusiveLock();
        del->lock.ReleaseExclusiveLock();
        next->lock.ReleaseExclusiveLock();
    } else {
        prev->lock.ReleaseSharedLock();
        next->lock.ReleaseSharedLock();
    }

    return result;
}

bool List::Search(const int key, char* data) const noexcept {
    if(data == nullptr) return false;

    NodePtr prev(head);
    prev->lock.LockRead();
    const NodePtr node(FindKey(prev, key, /*isRead = */true));

    bool result(node->key == key && node->isNodeActive && node != tail);
    if(result) {
        *data = node->data;
    }
    node->lock.ReleaseSharedLock();

    return result;
}

/**=============================================================================
 * End of file
 * ===========================================================================*/