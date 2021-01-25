/*--------------------------------------------------------------------------------------+
|
|     $Source: A3D/SqliteWrapper/SqliteWrapper.h $
|
|  $Copyright: (c) 2019 Bentley Systems, Incorporated. All rights reserved. $
|
+--------------------------------------------------------------------------------------*/

#pragma once
extern "C" {
    #include "sqlite3.h"
}
#include <limits.h>
#include <unordered_map>
#include <shared_mutex>

namespace A3D
{
    class SqliteWrapper
    {
    private:
        /*
        *   We had a bug in the way we prepare, step and reset or release our statements.
        *   We choosed to use sqlite_exec() API call to handle all the execution for us.
        */

        bool m_isOpened;
        unsigned int m_timeoutMs;                                    // Number of retries if DB is locked by another thread/process. 0 = forever.
        std::string m_databasePath;
        sqlite3* m_database;
        std::shared_mutex m_dbConnectionMutex;

        bool InitDatabase();
        bool DestroyDatabase();
        bool Reconnect();

    public:
        explicit SqliteWrapper(std::string const& databasePath);
        explicit SqliteWrapper(std::string const& databasePath, int timeoutMs);
        ~SqliteWrapper();

        //--------------------------------------------------------------------------------------
        // @description   Check if database is ready (in a good state) to be used.
        // @return        True if ready, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   08/19
        //+---------------+---------------+---------------+---------------+---------------+------
        bool IsReady() const;

        //--------------------------------------------------------------------------------------
        // @description Change retry timeout of each action (if DB is locked by another
        //              thread/process). 0 = forever.
        // @param       timeoutMs Timeout value (ms).
        // @bsimethod                                         Alexandre Gbaguidi A�sse   08/19
        //+---------------+---------------+---------------+---------------+---------------+------
        void SetTimeout(unsigned int timeoutMs);

        //--------------------------------------------------------------------------------------
        // @description Execute an SQL statement without saving it and get the results.
        // @param       statementTest   The request.
        // @param       retValue        Return code after execution.
        // @param       results         Filled with the results where each column has its vector,
        //                              the first column starting at 'results[0]'.
        // @param       pnUpdatedRows   If not null, return the number of rows actually updated by the execution of the statement
        // @return      True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool ExecStatement(const char* statementText, int& retValue, std::vector<std::vector<std::string>>& results, bool retry = true, int* pnUpdatedRows = nullptr);

        //--------------------------------------------------------------------------------------
        // @description Execute an SQL statement without saving it and return return code and number of modified rows
        // @param       statementText   The request.
        // @param       retValue        Return code after execution.
        // @param       nUpdatedRows    Return the number of rows actually updated by the execution of the statement
        // @return      True if everything went well, false otherwise.
        // @bsimethod                                         Jean-Philippe Pons   11/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool ExecStatement(const char* statementText, int& retValue, int& nUpdatedRows);

        //--------------------------------------------------------------------------------------
        // @description Execute an SQL statement without saving it and check return code.
        // @param       statementText   The request.
        // @param       retValue        Return code after execution.
        // @return      True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool ExecStatement(const char* statementText, int& retValue);

        //--------------------------------------------------------------------------------------
        // @description Execute an SQL statement without saving it.
        // @param       statementText   The request.
        // @return      True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool ExecStatement(const char* statementText);

        //--------------------------------------------------------------------------------------
        // @description   Start an SQL transaction. EndTransaction() must be called at the end.
        // @return        True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool BeginTransaction();

        //--------------------------------------------------------------------------------------
        // @description   Start an exclusive SQL transaction. EndTransaction() must be called
        //                at the end.
        // @return        True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool BeginExclusiveTransaction();

        //--------------------------------------------------------------------------------------
        // @description   Start an immediate SQL transaction. EndTransaction() must be called
        //                at the end.
        // @return        True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool BeginImmediateTransaction();

        //--------------------------------------------------------------------------------------
        // @description   End a previously started SQL Transaction.
        // @return        True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool EndTransaction();

        //--------------------------------------------------------------------------------------
        // @description   Cancel current transaction and roll back all the proposed changes.
        // @return        True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   04/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool RollBackTransaction();

        //--------------------------------------------------------------------------------------
        // @description   Check if database is currently in a transaction mode.
        // @return        True if everything went well, false otherwise.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   03/20
        //+---------------+---------------+---------------+---------------+---------------+------
        bool IsInTransaction();

        //--------------------------------------------------------------------------------------
        // @description   Return last message error triggered by Sqlite.
        // @return        A string with the message.
        // @bsimethod                                         Alexandre Gbaguidi A�sse   08/19
        //+---------------+---------------+---------------+---------------+---------------+------
        std::string LastErrorMessage();
    };
}
