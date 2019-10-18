#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <thread>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <set>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <stdarg.h>
#include <iostream>

class CThread
{
public:
    CThread() : m_bExit(true), m_pThread(nullptr) {}

    virtual ~CThread()
    {
        ThreadStop();
    }

    bool ThreadStart()
    {
        m_bExit = false;
        return (m_pThread = new std::thread(std::bind(&CThread::operator(), this))) != nullptr;
    }

    void ThreadStop()
    {
        if (m_pThread)
        {
            m_bExit = true;
            m_cond.notify_all();
            m_pThread->join();
            delete m_pThread;
            m_pThread = nullptr;
        }
    }

    virtual void operator()() = 0;

protected:
    bool m_bExit;
    std::thread *m_pThread;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define api_vsnprintf _vsnprintf
#else
#include <dirent.h>
#define api_vsnprintf vsnprintf
#endif

#define DEFAULT_MAX_KB		100 * 1024
#define MAX_LOG_BUFFER		100000

static const char *s_logAbbr[] = {"[F]", "[E]", "[W]", "[I]", "[D]", "[T]", nullptr};
static const char *s_logName[] = {"Force", "Error", "Warning", "Info", "Debug", "Trace", nullptr};

typedef enum
{
    LOG_FORCE = 0,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE
} LogLevel;

static std::string GetCurTime(bool bWithMSec = false)
{
    auto timepNow = std::chrono::system_clock::now();
    auto timetNow = std::chrono::system_clock::to_time_t(timepNow);

    std::stringstream strStream;
    strStream << std::put_time(std::localtime(&timetNow), "%Y-%m-%d %H:%M:%S");
    if (bWithMSec)
        strStream << "." << std::setfill('0') << std::setw(3) << timetNow % 1000;
    return strStream.str();
}

static inline size_t api_snprintf(char *pszBuff, size_t nSize, const char *pszFormat, ...)
{
    va_list argList;
    va_start(argList, pszFormat);
    int nLen = api_vsnprintf(pszBuff, nSize, pszFormat, argList);
    va_end(argList);
    return (nLen < 0 || (size_t)nLen >= nSize) ? 0 : (size_t)nLen;
}

class CLogger : public CThread
        {
                public:
                CLogger(const std::string &strFile = "")
                : m_eLevel(LOG_INFO), m_nMaxByte(DEFAULT_MAX_KB << 10), m_nMaxNum(10), m_bWithMSec(false)
                {
                    if (!strFile.empty())
                        Open(strFile);
                    m_pvecCurBuf = &m_vecBufA;
                    m_pvecBackupBuf = &m_vecBufB;
                }

                virtual ~CLogger()
                {
                    ThreadStop();
                    Close();
                }

                bool Open(const std::string &strFile)
                {
                    if (strFile.empty())
                        return false;

                    if (m_ofsLog && m_strFile == strFile)
                        return true;

                    m_strFile = strFile;

                    char ch;
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
                    ch = '\\';
#else
                    ch = '/';
#endif
                    auto pos = m_strFile.rfind(ch);
                    m_strLogDir = (pos == std::string::npos) ? "." : m_strFile.substr(0, pos + 1);
                    m_strLogName = (pos == std::string::npos) ? m_strFile : m_strFile.substr(pos + 1);
                    return Reopen() && ThreadStart();
                }

                void Close()
                {
                    if (m_ofsLog)
                    {
                        for (auto &strLog : *m_pvecCurBuf)
                        m_ofsLog << strLog << std::endl;

                        m_ofsLog.clear();
                        m_ofsLog.close();
                    }
                }

                bool IsOpen() { return m_ofsLog.is_open(); }
                void SetLogLevel(LogLevel eLevel) { m_eLevel = eLevel; }
                void SetMaxFileSize(int nMaxKb) { m_nMaxByte = nMaxKb << 10; }
                void SetMaxFileNum(int nMaxNum) { m_nMaxNum = nMaxNum; }
                void SetMilliSecondEnabled(bool bEnable) { m_bWithMSec = bEnable; }

#define MacroDefine(name, level) \
    void Log##name(const char *pszFormat, ...) \
    { \
        va_list argList; \
        va_start(argList, pszFormat); \
        Log(LOG_##level, pszFormat, argList); \
        va_end(argList); \
    }

                MacroDefine(Force, FORCE)
                MacroDefine(Error, ERROR)
                MacroDefine(Warning, WARNING)
                MacroDefine(Info, INFO)
                MacroDefine(Debug, DEBUG)
                MacroDefine(Trace, TRACE)
#undef MacroDefine

#define MacroDefine(name, level) \
	void Log##name(const std::string &strLog) \
	{ \
		Log(LOG_##level, strLog.c_str()); \
	}

                MacroDefine(Force, FORCE)
                MacroDefine(Error, ERROR)
                MacroDefine(Warning, WARNING)
                MacroDefine(Info, INFO)
                MacroDefine(Debug, DEBUG)
                MacroDefine(Trace, TRACE)
#undef MacroDefine

                private:
                void Log(LogLevel eLevel, const char *pszFormat, va_list argList)
                {
                    if (!m_ofsLog || m_eLevel < eLevel || !pszFormat || pszFormat[0] == 0)
                        return;

                    int nLen, nSize;
                    char szBuff[4096] = {0};
                    nSize = (int)api_snprintf(szBuff, sizeof(szBuff), "%s %s ", GetCurTime(m_bWithMSec).c_str(), s_logAbbr[eLevel]);
                    nLen = api_vsnprintf(&szBuff[nSize], sizeof(szBuff) - nSize, pszFormat, argList);
                    if (nLen <= 0)
                        return;

                    nLen += nSize;
                    szBuff[nLen] = 0;

                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (m_pvecCurBuf->size() < MAX_LOG_BUFFER)
                        m_pvecCurBuf->push_back(szBuff);
                }

                void Log(LogLevel eLevel, const std::string &strLog)
                {
                    if (!m_ofsLog || m_eLevel < eLevel || strLog.empty())
                        return;

                    std::lock_guard<std::mutex> lock(m_mutex);
                    std::string strBuff = GetCurTime(m_bWithMSec) + " " + s_logAbbr[eLevel] + " " + strLog;
                    if (m_pvecCurBuf->size() < MAX_LOG_BUFFER)
                        m_pvecCurBuf->push_back(strBuff);
                }

                bool Reopen()
                {
                    Close();
                    CleanFile();
                    m_ofsLog.open(m_strFile, std::ios::ate | std::ios::app);
                    if (!m_ofsLog)
                        return false;

                    if ((int)m_ofsLog.tellp() == 0)
                    {
                        m_ofsLog << std::setfill('#') << std::setw(55) << "" << std::endl;
                        m_ofsLog << "#  Abbreviations used in this document" << std::setfill(' ') << std::setw(17) << "#" << std::endl;
                        for (int i = 0; s_logAbbr[i] != NULL; i++)
                            m_ofsLog << "#  " << s_logAbbr[i] << "     " << s_logName[i] << std::setfill(' ')
                                     << std::setw(44 - strlen(s_logName[i])) << "#" << std::endl;
                        m_ofsLog << std::setfill('#') << std::setw(55) << "" << std::endl;
                    }
                    return true;
                }

                void CleanFile()
                {
                    std::set<std::string> setLogFile;
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#else
                    struct dirent *ptr;
                    DIR *dir = opendir(m_strLogDir.c_str());
                    while ((ptr = readdir(dir)) != NULL) {
                        if (m_strLogName != ptr->d_name &&
                            strncmp(ptr->d_name, m_strLogName.c_str(), m_strLogName.length()) == 0) {
                            setLogFile.emplace(ptr->d_name);
                        }
                    }
                    closedir(dir);
#endif

                    auto nCount = setLogFile.size();
                    for (auto strFile : setLogFile) {
                        if (nCount-- > (m_nMaxNum - 1)) {
                            remove((m_strLogDir + strFile).c_str());
                        }
                    }
                }

                bool Switch()
                {
                    Close();
                    std::string strTime = GetCurTime();
                    auto funcBePlace = [](char chVal) { return chVal == ' ' || chVal == ':' || chVal == '.'; };
                    std::replace_if(strTime.begin(), strTime.end(), funcBePlace, '-');
                    std::string strOldName = m_strFile + "_" + strTime;
                    return rename(m_strFile.c_str(), strOldName.c_str()) == 0 && Reopen();
                }

                virtual void operator()()
                {
                    while (!m_bExit)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            if (m_pvecCurBuf->empty())
                                continue;
                            else
                                std::swap(m_pvecCurBuf, m_pvecBackupBuf);
                        }

                        for (auto &strLog : *m_pvecBackupBuf)
                        {
                            if ((size_t)m_ofsLog.tellp() + strLog.length() > m_nMaxByte)
                            {
                                while (!Switch() && !m_bExit)
                                    std::this_thread::sleep_for(std::chrono::seconds(1));

                                if (m_bExit)
                                    break;
                            }
                            m_ofsLog << strLog << std::endl;
                        }
                        m_pvecBackupBuf->clear();
                    }
                }

                private:
                std::string m_strFile;
                std::ofstream m_ofsLog;
                LogLevel m_eLevel;
                size_t m_nMaxByte;
                unsigned int m_nMaxNum;
                bool m_bWithMSec;
                std::vector<std::string> m_vecBufA;
                std::vector<std::string> m_vecBufB;
                std::vector<std::string> *m_pvecCurBuf;
                std::vector<std::string> *m_pvecBackupBuf;

                std::string m_strLogDir;
                std::string m_strLogName;
        };

template <typename LOG>
LOG *& SingleLogger()
{
    static LOG *pLog = nullptr;
    if (!pLog)
        pLog = new CLogger();
    return pLog;
}

template <typename LOG>
void ReleaseLogger(LOG *& pLog)
{
if (pLog)
delete pLog;
pLog = nullptr;
}

#define _LOGGER_  (*(SingleLogger<CLogger>()))

#define INIT_LOGGER(file) \
do {\
    _LOGGER_.Open(file);\
} while (0)

#define UNINIT_LOGGER() do {\
    ReleaseLogger(SingleLogger<CLogger>());\
} while (0)

#define INIT_LOGGER_LEVEL(level) do {\
    _LOGGER_.SetLogLevel(level);\
} while (0)

#define INIT_LOGGER_MAXFILESIZE(maxkb) do {\
    _LOGGER_.SetMaxFileSize(maxkb);\
} while (0)

#define INIT_LOGGER_MAXFILENUM(maxNum) do {\
    _LOGGER_.SetMaxFileNum(maxNum);\
} while (0)

#define INIT_LOGGER_MSEC(enable) do {\
    _LOGGER_.SetMilliSecondEnabled(enable);\
} while (0)

#define LOGGING(type, message) do {\
    _LOGGER_.Log##type(message);\
} while (0)

#define LOG_FORCE(message) LOGGING(Force, message)
#define LOG_ERROR(message) LOGGING(Error, message)
#define LOG_WARNING(message) LOGGING(Warning, message)
#define LOG_INFO(message) LOGGING(Info, message)
#define LOG_DEBUG(message) LOGGING(Debug, message)
#define LOG_TRACE(message) LOGGING(Trace, message)

#endif // !_LOGGER_H_
