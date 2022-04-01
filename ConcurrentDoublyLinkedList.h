/*==============================================================================
 *******************************************************************************
 *==============================================================================
 * File name: ConcurrentDoublyLinkedList.h
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

#ifndef CONCURRENT_DOUBLY_LINKED_LIST_H_
#define CONCURRENT_DOUBLY_LINKED_LIST_H_

/**=============================================================================
 * Includes:
 * ===========================================================================*/

#include "ReadMayWriteWriteLock.h"

/**=============================================================================
 * Declarations:
 * ===========================================================================*/

/**
 * @brief A doubly-linked list, which is safe for concurrent operations.
 *        - The list acts as a map, where each node contains a key-value pair.
 *        - The list is kept sorted according the keys.
 */
class ConcurrentDoublyLinkedList {

/**-----------------------------------------------------------------------------
 * Private Definitions:
 * ---------------------------------------------------------------------------*/

    /**
     * @brief The concurrent doubly-linked list's node struct.
     */
    struct Node {
    
    /**-------------------------------------------------------------------------
     * Public Internal Variables:
     * -----------------------------------------------------------------------*/
        
        /**
         * @brief The key of the node.
         */
        const int key;

        /**
         * @brief The data of the node.
         */
        const char data;
        
        /**
         * @brief A pointer to the previous node in the list.
         */
        shared_ptr<Node> prevPtr;

        /**
         * @brief A pointer to the next node in the list.
         */
        shared_ptr<Node> nextPtr;
        
        /**
         * @brief Due to concurrency, a thread can hold a pointer to a node
         *        which was removed from the list. This flag tells the state of
         *        the node.
         */
        bool isNodeActive;
        
        /**
         * @brief A personal Read/May-Write/Write lock for the node.
         */
        Read_MayWrite_Write_Lock lock;
        
    /**-------------------------------------------------------------------------
     * Public Methods:
     * -----------------------------------------------------------------------*/

        /**
         * @brief Construct a new Node object.
         * 
         * @param in_key     New node's key.
         * @param in_data    New node's data.
         * @param in_prevPtr A pointer to the previous node.
         * @param in_nextPtr A pointer to the next node.
         */
        Node(const int in_key,
             const char in_data,
             const shared_ptr<Node>& in_prevPtr = nullptr,
             const shared_ptr<Node>& in_nextPtr = nullptr);
    };

    typedef shared_ptr<Node> NodePtr;

/**-----------------------------------------------------------------------------
 * Private Internal Variables:
 * ---------------------------------------------------------------------------*/

    /**
     * @brief This node acts as the head of the list.
     *        - Its next node is the one with the lowest key value.
     *        - Its previous node is nullptr.
     */
    const NodePtr head;

    /**
     * @brief This node acts as the tail of the list.
     *        - Its next node is nullptr.
     *        - Its previous node is the one with the highest key value.
     */
    const NodePtr tail;

/**-----------------------------------------------------------------------------
 * Private Service Methods:
 * ---------------------------------------------------------------------------*/

    /**
     * @brief Advancing towards the tail one place, acquiring and releasing
     *        appropriate locks in the process. Returning the updated nodes as
     *        an input/output variables.
     * 
     * @attention It is assumed that the thread executing this method holds the
     *            lock of the prev in a read/may-write mode.
     * @attention It is assumed that the prev node is not the tail.
     * @attention Depending on the flag boolean isRead, the locks of the output
     *            prev and next nodes are acquired when the method exits. Make
     *            sure to release it/them.
     * 
     * @param prev   A node to advance such tat it points to next at the end.
     * @param next   A node to advance by one place.
     * @param isRead Specifies the type of lock acquiring method.
     *               - If true, locks are acquired in read mode, where
     *                 throughout the execution, there is at most one lock
     *                 which is acquired. At the end, the lock of position is
     *                 released, and only the lock of the candidate node is
     *                 held.
     *               - If false, locks are acquired in may-write mode. At the
     *                 end, both locks of the position and candidate nodes are
     *                 held.
     */
    void AdvanceAndLockReadMayWrite(NodePtr& prev,
                                    NodePtr& next,
                                    const bool isRead) const noexcept;

    /**
     * @brief Starting at a given position in the list, advancing towards the
     *        tail, acquiring and releasing appropriate locks in the process,
     *        until finding the first node with key that is larger or equal to
     *        the key that is looked for. Returning this node (call it candidate
     *        node) as a return value, and also updating the position node,
     *        which is an input/output variable, to point to the node which is
     *        previous to the candidate.
     * 
     * @attention It is assumed that the thread executing this method holds the
     *            lock of the position in a read/may-write mode.
     * @attention It is assumed that the position node is active.
     * @attention It is assumed that the position node is not the tail.
     * @attention Depending on the flag boolean isRead, the locks of the output
     *            candidate node and the position node are acquired when the
     *            method exits. Make sure to release it/them.
     * 
     * @param position An input/output parameter. It points to a node in the
     *                 list from which the search of the key should start.
     * @param key      The key which is looked for in the list.
     * @param isRead   Specifies the type of lock acquiring method.
     *                 - If true, locks are acquired in read mode, where
     *                   throughout the execution, there is at most one lock
     *                   which is acquired. At the end, the lock of position is
     *                   released, and only the lock of the candidate node is
     *                   held.
     *                 - If false, locks are acquired in may-write mode. At the
     *                   end, both locks of the position and candidate nodes are
     *                   held.
     *  
     * @retval NodePtr A pointer to a candidate node - it is the first node with
     *                 its key larger or equal to the key that is looked for.
     */
    NodePtr FindKey(NodePtr& position,
                    const int key,
                    const bool isRead = false) const noexcept;
    
    /**
     * @brief Inserts the key, with the appropriate data, into the ordered
     *        doubly-linked list. The search for the appropriate location in the
     *        list starts from the given position, and the advancement is
     *        towards the list's tail. If the key already exists in the list, no
     *        insertion is done.
     * 
     * @attention It is assumed that the thread executing this method holds the
     *            lock of the position node in a May-Write mode.
     * @attention It is assumed that the position node is active.
     * @attention It is assumed that the position node is not the tail.
     * 
     * @param key      New node's key.
     * @param data     New node's data.
     * @param position The position from which the operation starts.
     * 
     * @retval true  If the key and value were inserted to the list.
     * @retval false If the key was already existing in the list.
     */
    bool InsertFromPosition(const NodePtr& position,
                            const int key,
                            const char data);

/**-----------------------------------------------------------------------------
 * Public Methods:
 * ---------------------------------------------------------------------------*/

public:

    /**
     * @brief The list's constructor.
     */
    ConcurrentDoublyLinkedList();

    /**
     * @brief The list's destructor.
     */
    ~ConcurrentDoublyLinkedList() noexcept;
    
    /**
     * @brief Inserts the key, with the appropriate data, into the ordered
     *        doubly-linked list. The search for the appropriate location in the
     *        list starts from the head of the list. If the key already exists
     *        in the list, no insertion is done.
     * 
     * @param key  New node's key.
     * @param data New node's data.
     * 
     * @retval true  If the key and value were inserted to the list.
     * @retval false If the key was already existing in the list.
     */
    bool InsertHead(const int key, const char data);
    
    /**
     * @brief Inserts the key, with the appropriate data, into the ordered
     *        doubly-linked list. The search for the appropriate location in the
     *        list starts from the tail of the list. If the key already exists
     *        in the list, no insertion is done.
     * 
     * @param key  New node's key.
     * @param data New node's data.
     * 
     * @retval true  If the key and value were inserted to the list.
     * @retval false If the key was already existing in the list.
     */
    bool InsertTail(const int key, const char data);

    /**
     * @brief Deletes the key, with the appropriate data, from the ordered
     *        doubly-linked list. The search for the appropriate location in the
     *        list starts from the head of the list.
     * 
     * @param key The key of the node that should be deleted.
     * 
     * @retval true  If the relevant node was deleted from the list.
     * @retval false If the key does not existing in the list.
     */
    bool Delete(const int key) noexcept;

    /**
     * @brief Determines whether the key exists in the ordered doubly-linked
     *        list. The search for the appropriate location in the list starts
     *        from the head of the list. If the key is found, its associated
     *        data is returned.
     * 
     * @param key  The key of the node to look for.
     * @param data An output parametr, to which the data should be written.
     * 
     * @retval true  If the operation was successful and the data was retrieved.
     * @retval false If the key does not existing in the list or the output
     *               parameter is invalid.
     */
    bool Search(const int key, char* data) const noexcept;
};

/**=============================================================================
 * End of file
 * ===========================================================================*/

#endif /* CONCURRENT_DOUBLY_LINKED_LIST_H_ */