/*==============================================================================
 *******************************************************************************
 *==============================================================================
 * File name: Test.cpp
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
#include <string>
#include <iostream>
#include <random>
#include <cassert>
#include <chrono>

using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::scoped_lock;
using std::random_device;
using std::mt19937;
using std::uniform_int_distribution;

/**=============================================================================
 * Definitions:
 * ===========================================================================*/

typedef ConcurrentDoublyLinkedList List;

/**
 * @brief Enumeration type for the different operations on the list.
 */
enum Operation {INSERT_HEAD, INSERT_TAIL, DELETE, SEARCH};

/**=============================================================================
 * Declarations:
 * ===========================================================================*/

/**
 * @brief Synchronizes the standard output printing.
 * 
 * @param text The text to print.
 */
void SafePrint(const std::string& text);

/**
 * @brief Compiles a text that states the thread's identity and the operation to
 *        perform on the list.
 * 
 * @param threadID Thread identifier.
 * @param key      The key argument for the list operation.
 * @param data     The data argument for the list operation.
 * @param op       The operation to perform on the list.
 * 
 * @return A string which represents the thread and its job.
 */
string GetOperation(const string& threadID,
                    const string& key,
                    const string& data,
                    const Operation op);

/**
 * @brief Prints the operation that should be performed by the thread.
 * 
 * @param threadID Thread identifier.
 * @param key      The key argument for the list operation.
 * @param data     The data argument for the list operation.
 * @param op       The operation to perform on the list.
 */
void PrintOperation(const string& threadID,
                    const string& key,
                    const string& data,
                    const Operation op);

/**
 * @brief Prints the operation that should be performed by the thread, and its
 *        result.
 * 
 * @param threadID Thread identifier.
 * @param key      The key argument for the list operation.
 * @param data     The data argument for the list operation.
 * @param op       The operation to perform on the list.
 */
void PrintOperationResult(const string& threadID,
                          const string& key,
                          const string& data,
                          const Operation op,
                          const bool result);

/**
 * @brief Acts as a barrier for the threads. Causes them to wait on the same
 *        condition variable.
 * 
 * @param threadID Thread identifier.
 */
void Wait(const string& threadID);

/**
 * @brief The last thread that enters here will wake up the parent thread.
 * 
 * @param threadID Thread identifier.
 */
void Finish(const string& threadID);

/**
 * @brief Generates a list task randomly.
 * 
 * @param threadID Thread identifier.
 */
void ThreadTask(const string&& threadID);

/*==============================================================================
 * Global Variables:
 *============================================================================*/

const unsigned int       MAX_THREADS(1000);
unsigned int             threadCounter(0);
List                     clist;
condition_variable       childrenCondition,
                         parentCondition;
mutex                    globalMutex,
                         printMutex;
random_device            rd;
mt19937                  generator(rd());
uniform_int_distribution randomKey(1, 100),
                         randomData(33, 126),
                         randomOperation(static_cast<int>(INSERT_HEAD),
                                         static_cast<int>(SEARCH));
bool                     ready(false);

/*==============================================================================
 * Implementation:
 *============================================================================*/

void SafePrint(const string& text) {
    scoped_lock<mutex> lock(printMutex);
    cout << text << endl;
}

string GetOperation(const string& threadID,
                    const string& key,
                    const string& data,
                    const Operation op) {
    const string operations[4]{"InsertHead", "InsertTail", "Delete", "Search"};
    
    string result(threadID + ": " + operations[op] + "(" + key);
    switch(op) {
        case DELETE:
            result += ")";
            break;
        case SEARCH:
            result += ", &data)";
            break;
        default:
            result += ", " + data + ")";
    }
    
    return result;
}

void PrintOperation(const string& threadID,
                    const string& key,
                    const string& data,
                    const Operation op) {
    SafePrint(GetOperation(threadID, key, data, op));
}

void PrintOperationResult(const string& threadID,
                          const string& key,
                          const string& data,
                          const Operation op,
                          const bool result) {
    string suffix(result ? "true" : "false");
    if(op == SEARCH && result) {
        suffix += ", data = " + data;
    }
    SafePrint(GetOperation(threadID, key, data, op) + " - " + suffix);
}

void Wait(const string& threadID) {
    unique_lock<mutex> lock(globalMutex);

    ++threadCounter;
    if(threadCounter == MAX_THREADS) {
        SafePrint(threadID + ": We are all ready. Waking up parent thread.");
        parentCondition.notify_one();
    }
    childrenCondition.wait(lock, []{return ready;});
}

void Finish(const string& threadID) {
    scoped_lock<mutex> lock(globalMutex);

    --threadCounter;
    if(threadCounter == 0) {
        SafePrint(threadID + ": We all finished. Waking up parent thread.");
        parentCondition.notify_one();
    }
}

void ThreadTask(const string&& threadID) {
    const int key(randomKey(generator));
    char data(static_cast<char>(randomData(generator)));
    const Operation op(static_cast<Operation>(randomOperation(generator)));

    const string keyStr(to_string(key));
    bool result(false);

    Wait(threadID);

    PrintOperation(threadID, keyStr, string() + data, op);
    switch(op) {
        case INSERT_HEAD:
            result = clist.InsertHead(key, data);
            break;
        case INSERT_TAIL:
            result = clist.InsertTail(key, data);
            break;
        case DELETE:
            result = clist.Delete(key);
            break;
        case SEARCH:
            result = clist.Search(key, &data);
            break;
        default:
            // Should not arrive here.
            assert(op != INSERT_HEAD && \
                   op != INSERT_TAIL && \
                   op != DELETE      && \
                   op != SEARCH);
    }
    PrintOperationResult(threadID, keyStr, string() + data, op, result);

    Finish(threadID);
}

int main() {
    SafePrint("Test started.");
    
    unique_lock<mutex> lock(globalMutex);
    
    assert(!ready);
    for(unsigned int i = 0; i < MAX_THREADS; ++i) {
        thread(ThreadTask, to_string(i + 1)).detach();
    }
    SafePrint("All threads created. Waiting for them.");

    parentCondition.wait(lock, []{return threadCounter == MAX_THREADS;});
    
    SafePrint("Releasing threads and waiting for them to finish.");
    ready = true;
    childrenCondition.notify_all();
    parentCondition.wait(lock, []{return threadCounter == 0;});

    SafePrint("Test ended successfully.");

    return 0;
}

/**=============================================================================
 * End of file
 * ===========================================================================*/