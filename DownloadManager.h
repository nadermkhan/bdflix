#pragma once
#include "pch.h"
#include "Models.h"
#include "SearchEngine.h"   // for g_ConnPool
#include "Helpers.h"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <winhttp.h>

namespace BDFlix
{
    using DlUpdateCallback =
        std::function<void(int id, DLState state,
            long long downloaded, long long total,
            double speed)>;

    using DlCompleteCallback =
        std::function<void(int id, DLState finalState)>;

    class DownloadManager
    {
    public:
        DownloadManager() = default;
        ~DownloadManager();

        int QueueDownload(const std::wstring& url,
            const std::wstring& fileName,
            DlUpdateCallback onUpdate,
            DlCompleteCallback onComplete);

        void Pause(int id);
        void Resume(int id);
        void Cancel(int id);
        void Remove(int id);       // only when done/error/cancelled
        void UpdateSpeeds();       // call from UI timer

        std::vector<std::shared_ptr<DownloadTask>>
            GetTasks() const;      // returns snapshot

        std::shared_ptr<DownloadTask> FindTask(int id) const;

    private:
        bool ParseUrl(const std::wstring& url,
            std::wstring& host,
            std::wstring& path, int& port);

        long long HttpFileSize(const std::wstring& h, int p,
            const std::wstring& path);

        bool HttpRangeOk(const std::wstring& h, int p,
            const std::wstring& path);

        void StartTask(std::shared_ptr<DownloadTask> t,
            DlUpdateCallback onUpdate,
            DlCompleteCallback onComplete);

        void SingleDl(std::shared_ptr<DownloadTask> t,
            DlUpdateCallback onUpdate,
            DlCompleteCallback onComplete);

        void ChunkWork(std::shared_ptr<DownloadTask> t,
            int chunkIdx);

        mutable std::recursive_mutex m_mutex;
        std::vector<std::shared_ptr<DownloadTask>> m_tasks;
        int m_nextId{ 1 };
    };

    // Global instance
    inline DownloadManager g_DlManager;
}
