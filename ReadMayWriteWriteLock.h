/*==============================================================================
 *******************************************************************************
 *==============================================================================
 * File name: ReadMayWriteWriteLock.h
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

#ifndef READ_MAYWRITE_WRITE_LOCK_H_
#define READ_MAYWRITE_WRITE_LOCK_H_

/**=============================================================================
 * Includes:
 * ===========================================================================*/

#include <mutex>
#include <condition_variable>
#include <tuple>
#include <list>
#include <memory>
#include <thread>

using std::condition_variable;
using std::mutex;
using std::unique_lock;
using std::tuple;
using std::list;
using std::shared_ptr;
using std::thread;

/**=============================================================================
 * Declarations:
 * ===========================================================================*/

/**
 * @brief A fair read/may-write/write lock class.
 * 
 * Behavior:
 *  - The lock is a fair lock. It implements a First-In-First-Out queue, with
 *    respect to arrival time of the threads.
 *  - An unlimited number of readers can acquire the lock (as long as it is
 *    fair).
 *  - Only one may-write thread can acquire the lock. No limitation on the
 *    number of readers acquiring the lock simultaneously to a may-writer.
 *  - Only one writer may acquire the lock. When a writer has the lock, all
 *    other threads are blocked.
 *
 * @attention There is no check for the validity of the requests. It is assumed
 *            that whoever initiates a request, has the lock in the right state.
 *            For example:
 *            - If a thread calls the upgradeLock method, it is assumed that it
 *              already holds the lock in a may-write mode.
 *            - If one of the realese methods is called, the assumption is that
 *              a thread who holds the lock calls the correct method
 *              (ReleaseSharedLock for read/may-write and ReleaseExclusiveLock
 *              for write).
 */
class Read_MayWrite_Write_Lock {

/**-----------------------------------------------------------------------------
 * Private Definitions:
 * ---------------------------------------------------------------------------*/

    /**
     * @brief Enumeration type for the different modes of acquiring the lock.
     */
    enum Operation {READ, MAY_WRITE, WRITE};

    typedef shared_ptr<condition_variable> ConditionPtr;
    typedef tuple<ConditionPtr, Operation, unsigned int> ConditionTuple;

/**-----------------------------------------------------------------------------
 * Private Internal Variables:
 * ---------------------------------------------------------------------------*/

    /**
     * @brief An internal mutex, to protect the internal variables.
     */
    mutex internalMutex;

    /**
     * @brief A queue, to ensure a fair lock (as much as possible).
     *        Generally speaking, the queue contains a condition variable for
     *        which a thread or a group of threads are waiting.
     * 
     * @remark Although it contains condition variables, these variables
     *         represent the relative position of threads in a queue for
     *         acquiring the lock, so this is the reason it is called a thread
     *         queue.
     */
    list<ConditionTuple> threadQueue;

    /**
     * @brief Number of readers currenty holding the lock (including the
     *        may-writer!).
     * 
     * @remark Including the may-writer in this counting is not mandatory. It is
     *         done solely for the case when all the readers release the lock
     *         while there is still a may-writer holding it. In this case, the
     *         last reader will not have to check whether a waiting thread
     *         should be awakened, as a waiting thread can only be a may-writer
     *         or a writer, which are not allowed to hold the lock.
     */
    unsigned int readersNumber;
    
    /**
     * @brief A boolean that indicates whether a writer is currently holding the
     *        lock.
     */
    bool isWriterHolding;

    /**
     * @brief A thread identifier of the thread that holds the lock in a
     *        may-write mode. If no such thread exists, the identifier does not
     *        represent a thread.
     */
    thread::id mayWriterThreadId;

/**-----------------------------------------------------------------------------
 * Private Service Methods:
 * ---------------------------------------------------------------------------*/

    /**
     * @brief A service method for read condition check.
     * 
     * @retval true  If no writer is holding the lock.
     * @retval false If a writer is holding the lock.
     */
    bool CanReaderAcquireLock() const noexcept;

    /**
     * @brief A service method for may-write condition check.
     * 
     * @retval true  If no may-writer and writer are holding the lock.
     * @retval false If a may-writer or a writer is holding the lock.
     */
    bool CanMayWriterAcquireLock() const noexcept;

    /**
     * @brief A service method for write condition check.
     * 
     * @retval true  If no one is holding the lock.
     * @retval false If someone is holding the lock.
     */
    bool CanWriterAcquireLock() const noexcept;

    /**
     * @brief A service method for condition check depending on the operation.
     * 
     * @param operation The acquiring mode for which the check is made.
     * 
     * @retval true  The lock can be acquired for the specified operation.
     * @retval false The lock can not be acquired for the specified operation.
     */
    bool CanAcquireLock(const Operation operation) const noexcept;

    /**
     * @brief A service method that creates a condition variable for a thread
     *        that should wait, and inserting it into the thread queue.
     * 
     * @param operation The acquiring mode of the lock.
     * @param isVIP     If true, this insertion is to the start of the queue.
     *                  Otherwise, the condition variable is added to the back.
     */
    void InsertConditionTuple(const Operation operation,
                              const bool isVIP = false);

    /**
     * @brief A service method that checks whether a thread can acquire the lock
     *        or it should wait for it. In case that a wait is needed, a
     *        condition variable is created, and pushed to the back of the
     *        thread queue (if needed).
     * 
     * @param operation The acquiring mode of the lock.
     * 
     * @retval true  The thread should wait on the condition variable for the
     *               lock.
     * @retval false The thread should not wait on the condition variable for
     *               the lock.
     */
    bool ShouldThreadWait(const Operation operation);

    /**
     * @brief A service method that makes a thread wait for its condition
     *        variable.
     * 
     * @param operation The acquiring mode of the lock.
     */
    void Wait(const Operation operation, unique_lock<mutex>& lock) noexcept;

    /**
     * @brief A service method that wakes the next thread(s) in the thread queue
     *        (through their condition variable) if they can acquire the lock.
     */
    void TryNotifyingNext() const noexcept;

/**-----------------------------------------------------------------------------
 * Public Methods:
 * ---------------------------------------------------------------------------*/

public:

    /**
     * @brief The lock's constructor.
     */
    Read_MayWrite_Write_Lock();

    /**
     * @brief Locks the lock in a read mode.
     *        Allowed if there are currently just readers and/or one may-writer
     *        which did not request an upgrade.
     */
    void LockRead();

    /**
     * @brief Locks the lock in a may-write mode.
     *        Allowed if there are just readers in the lock, and no additional
     *        may-writers. Also there should be no writers or a may-writer that
     *        waits for an upgrade.
     */
    void LockMayWrite();

    /**
     * @brief Locks the lock in a write mode.
     *        Allowed if no one is holding the lock.
     */
    void LockWrite();

    /**
     * @brief Upgrades the lock from a may-write to a write mode.
     *        This upgrade has a priority over any other waiting threads.
     *        The thread waiting for an upgrade will, in turn, need to wait till
     *        all current readers release the lock.
     */
    void UpgradeLock();

    /**
     * @brief Releases the lock that was acquired in shared (read/may-write)
     *        mode.
     */
    void ReleaseSharedLock() noexcept;

    /**
     * @brief Releases the lock that was acquired in exclusive (write) mode.
     */
    void ReleaseExclusiveLock() noexcept;
};

/**=============================================================================
 * End of file
 * ===========================================================================*/

#endif /* READ_MAYWRITE_WRITE_LOCK_H_ */