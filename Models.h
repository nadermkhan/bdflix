#pragma once
#include "pch.h"
#include <string>
#include <atomic>
#include <vector>

namespace BDFlix
{
    struct ServerInfo
    {
        std::wstring name;
        std::wstring host;
        std::wstring path;
        int port{ 80 };
    };

    struct FileResult
    {
        std::wstring name;
        std::wstring href;
        std::wstring fullUrl;
        std::wstring size;
        std::wstring server;
        std::wstring folder;
        std::wstring ext;
        long long sizeBytes{ -1 };
    };

    enum class DLState
    {
        Queued,
        Downloading,
        Paused,
        Complete,
        Error,
        Cancelled
    };

    struct DownloadChunk
    {
        long long startByte{ 0 };
        long long endByte{ 0 };
        std::atomic<long long> downloaded{ 0 };
        std::atomic<bool> done{ false };

        DownloadChunk() = default;
        DownloadChunk(DownloadChunk&& o) noexcept
        {
            startByte = o.startByte;
            endByte = o.endByte;
            downloaded.store(o.downloaded.load());
            done.store(o.done.load());
        }
        DownloadChunk& operator=(DownloadChunk&& o) noexcept
        {
            startByte = o.startByte;
            endByte = o.endByte;
            downloaded.store(o.downloaded.load());
            done.store(o.done.load());
            return *this;
        }
    };

    struct DownloadTask
    {
        int id{ 0 };
        std::wstring url;
        std::wstring fileName;
        std::wstring savePath;
        std::wstring host;
        std::wstring path;
        int port{ 80 };
        long long fileSize{ -1 };
        std::atomic<long long> totalDl{ 0 };
        std::atomic<DLState> state{ DLState::Queued };
        std::atomic<bool> paused{ false };
        std::atomic<bool> cancelled{ false };
        int nThreads{ 8 };
        std::vector<DownloadChunk> chunks;
        DWORD startTime{ 0 };
        long long lastBytes{ 0 };
        DWORD lastTime{ 0 };
        double speed{ 0 };
        double smoothSpeed{ 0 };
    };

    // Observable wrapper for UI binding
    struct DownloadItemVM :
        winrt::implements<DownloadItemVM,
            winrt::Windows::Foundation::IInspectable>
    {
        std::shared_ptr<DownloadTask> task;

        explicit DownloadItemVM(std::shared_ptr<DownloadTask> t)
            : task(std::move(t)) {}
    };
}
