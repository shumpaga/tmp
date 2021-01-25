/*--------------------------------------------------------------------------------------+
|
|     $Source: A3D/SqliteWrapper/SqliteWrapper.cpp $
|
|  $Copyright: (c) 2019 Bentley Systems, Incorporated. All rights reserved. $
|
+--------------------------------------------------------------------------------------*/

#include "SqliteWrapper.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace A3D
{
    bool SqliteWrapper::InitDatabase()
    {
        if (m_database)
        {
            return false;
        }

        // Open or create DB
        if (SQLITE_OK != sqlite3_a3d_open_v2(m_databasePath.c_str(), &m_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr))
        {
            return false;
        }
        sqlite3_a3d_busy_timeout(m_database, 2000);
        m_isOpened = true;

        return m_isOpened;
    }

    bool SqliteWrapper::Reconnect()
    {
        m_dbConnectionMutex.lock();
        sqlite3_a3d_close_v2(m_database);
        if (SQLITE_OK != sqlite3_a3d_open_v2(m_databasePath.c_str(), &m_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr))
        {
            m_dbConnectionMutex.unlock();
            return false;
        }
        m_dbConnectionMutex.unlock();

        return true;
    }

    bool SqliteWrapper::DestroyDatabase()
    {
        if (!m_database || !m_isOpened)
        {
            return false;
        }

        if (IsInTransaction() && !RollBackTransaction())
            std::cout << "DestroyDatabase: could not rollback transaction" << std::endl;

        sqlite3_a3d_close_v2(m_database);

        m_isOpened = false;
        m_database = nullptr;
        return true;
    }

    /************************** Public Methods ***************************/

    SqliteWrapper::SqliteWrapper(std::string const& databasePath)
        : m_isOpened(false),
        m_timeoutMs(3000), // Timeout after 3s
        m_databasePath(databasePath),
        m_database(nullptr)
    {
#ifdef LINUX
        SetTimeout(30000);
#endif
        InitDatabase();
    }

    SqliteWrapper::SqliteWrapper(std::string const& databasePath, int timeoutMs)
        : m_isOpened(false),
        m_timeoutMs(timeoutMs),
        m_databasePath(databasePath),
        m_database(nullptr)
    {
        InitDatabase();
    }

    SqliteWrapper::~SqliteWrapper()
    {
        DestroyDatabase();
    }

    bool SqliteWrapper::IsReady() const
    {
        return m_isOpened;
    }

    void SqliteWrapper::SetTimeout(unsigned int timeoutMs)
    {
        m_timeoutMs = timeoutMs;
    }

    static int getResultsCallBack(void* container, int count, char** data, char** columns)
    {
        std::vector<std::vector<std::string>>* results = reinterpret_cast<std::vector<std::vector<std::string>>*>(container);
        results->resize(count);
        for (int i = 0; i < count; ++i)
        {
            if (data[i])
            {
                (*results)[i].push_back(std::string(data[i]));
            }
            else // NULL value in SQLite column
            {
                (*results)[i].push_back(std::string(""));
            }
        }
        return 0;
    }

    bool SqliteWrapper::ExecStatement(const char* statementText, int& retValue, std::vector<std::vector<std::string>>& results, bool retry, int* pnUpdatedRows)
    {

        if (!IsReady())
            return false;

        bool alreadyTriedReconnecting = false;
        bool done = false;
        bool firstRowIteration = true;
        int numberOfColumn = 0;
        int i = 0;
        while (!done)
        {
            // Several executations can happen in parallel if we do not need the atomicity of sqlite3_a3d_exec followed by sqlite3_a3d_changes
            if (pnUpdatedRows != nullptr)
            {
                *pnUpdatedRows = 0;
                m_dbConnectionMutex.lock();
            }
            else
            {
                m_dbConnectionMutex.lock_shared();
            }

            retValue = sqlite3_a3d_exec(m_database, statementText, getResultsCallBack, &results, nullptr);

            if (pnUpdatedRows != nullptr)
            {
                if (retValue == 0)
                {
                    *pnUpdatedRows = sqlite3_a3d_changes(m_database);
                    if (*pnUpdatedRows < 0)
                    {
                    }
                }
                m_dbConnectionMutex.unlock();
            }
            else
            {
                m_dbConnectionMutex.unlock_shared();
            }

            switch (retValue)
            {
            case SQLITE_BUSY:
                if (m_timeoutMs == 0 || retry && i++ < 10)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                }
                else if (alreadyTriedReconnecting)
                {
                    done = true;
                }
                else
                {
                    done = !Reconnect();
                    alreadyTriedReconnecting = true;
                }
                break;

            case 0:  // Success
                done = true;
                break;

            case SQLITE_IOERR:
                if (alreadyTriedReconnecting)
                {
                    done = true;
                    break;
                }
                done = !Reconnect();
                alreadyTriedReconnecting = true;
                break;

            default:
                std::cout << "WrapperExecError: " << LastErrorMessage() << std::endl;
                if (alreadyTriedReconnecting)
                {
                    done = true;
                    break;
                }
                done = !Reconnect();
                alreadyTriedReconnecting = true;
                break;
            }
        }

        return retValue == 0;
    }

    bool SqliteWrapper::ExecStatement(const char* statementText, int& retValue)
    {
        std::vector<std::vector<std::string>> results;
        return ExecStatement(statementText, retValue, results, true, nullptr);
    }

    bool SqliteWrapper::ExecStatement(const char* statementText, int& retValue, int& nUpdatedRows)
    {
        std::vector<std::vector<std::string>> results;
        return ExecStatement(statementText, retValue, results, true, &nUpdatedRows);
    }

    bool SqliteWrapper::ExecStatement(const char* statementText)
    {
        int retValue;
        std::vector<std::vector<std::string>> results;
        return ExecStatement(statementText, retValue, results, true, nullptr);
    }

    bool SqliteWrapper::BeginTransaction()
    {
        return ExecStatement("BEGIN TRANSACTION");
    }

    bool SqliteWrapper::BeginExclusiveTransaction()
    {
        return ExecStatement("BEGIN EXCLUSIVE TRANSACTION");
    }

    bool SqliteWrapper::BeginImmediateTransaction()
    {
        int retValue = 0;
        std::vector<std::vector<std::string>> results;
        return ExecStatement("BEGIN IMMEDIATE TRANSACTION", retValue, results, true);
    }

    bool SqliteWrapper::EndTransaction()
    {
        int retValue = 0;
        std::vector<std::vector<std::string>> results;
        return ExecStatement("COMMIT", retValue, results, true);
    }

    bool SqliteWrapper::RollBackTransaction()
    {
        int retValue = 0;
        std::vector<std::vector<std::string>> results;
        return ExecStatement("ROLLBACK TRANSACTION", retValue, results, true);
    }

    bool SqliteWrapper::IsInTransaction()
    {
        /*
        *   The sqlite3_get_autocommit() interface returns non-zero or zero if the given database connection
        *   is or is not in autocommit mode, respectively. Autocommit mode is on by default.
        *   Autocommit mode is disabled by a BEGIN statement. Autocommit mode is re-enabled by a COMMIT or ROLLBACK.
        */
        return 0 == sqlite3_a3d_get_autocommit(m_database);
    }

    std::string SqliteWrapper::LastErrorMessage()
    {
        return sqlite3_a3d_errmsg(m_database);
    }
}
