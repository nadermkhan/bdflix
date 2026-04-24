#include "pch.h"
#include "DownloadManager.h"
#include <thread>
#include <algorithm>

namespace BDFlix
{
    DownloadManager::~DownloadManager()
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        for (auto& t : m_tasks) t->cancelled = true;
        Sleep(300);
    }

    int DownloadManager::QueueDownload(
        const std::wstring& url,
        const std::wstring& fileName,
        DlUpdateCallback onUpdate,
        DlCompleteCallback onComplete)
    {
        auto t = std::make_shared<DownloadTask>();
        {
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
            t->id       = m_nextId++;
            t->url      = url;
            t->fileName = fileName;
            t->savePath = Helpers::DlFolder() + L"\\" +
                Helpers::SanitizeFN(fileName);
            t->nThreads = 8;
            m_tasks.push_back(t);
        }
        StartTask(t, onUpdate, onComplete);
        return t->id;
    }

    void DownloadManager::Pause(int id)
    {
        auto t = FindTask(id);
        if (t && t->state.load() == DLState::Downloading)
        {
            t->paused = true;
            t->state  = DLState::Paused;
        }
    }

    void DownloadManager::Resume(int id)
    {
        auto t = FindTask(id);
        if (t && t->state.load() == DLState::Paused)
        {
            t->paused = false;
            t->state  = DLState::Downloading;
        }
    }

    void DownloadManager::Cancel(int id)
    {
        auto t = FindTask(id);
        if (t)
        {
            t->cancelled = true;
            t->state     = DLState::Cancelled;
        }
    }

    void DownloadManager::Remove(int id)
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
            [id](const std::shared_ptr<DownloadTask>& t)
            { return t->id == id; });
        if (it != m_tasks.end())
        {
            auto st = (*it)->state.load();
            if (st == DLState::Complete ||
                st == DLState::Error    ||
                st == DLState::Cancelled)
            {
                m_tasks.erase(it);
            }
        }
    }

    void DownloadManager::UpdateSpeeds()
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        DWORD now = GetTickCount();
        for (auto& t : m_tasks)
        {
            if (t->state.load() != DLState::Downloading) continue;
            if (t->lastTime > 0 && now > t->lastTime)
            {
                double el = (now - t->lastTime) / 1000.0;
                if (el > 0.05)
                {
                    long long dl = t->totalDl.load();
                    double inst = (double)(dl - t->lastBytes) / el;
                    t->smoothSpeed = (t->smoothSpeed <= 0)
                        ? inst
                        : t->smoothSpeed * 0.7 + inst * 0.3;
                    t->speed     = t->smoothSpeed;
                    t->lastBytes = dl;
                }
            }
            t->lastTime = now;
        }
    }

    std::vector<std::shared_ptr<DownloadTask>>
        DownloadManager::GetTasks() const
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        return m_tasks; // shared_ptr copy is safe
    }

    std::shared_ptr<DownloadTask>
        DownloadManager::FindTask(int id) const
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        for (auto& t : m_tasks)
            if (t->id == id) return t;
        return nullptr;
    }

    bool DownloadManager::ParseUrl(
        const std::wstring& url,
        std::wstring& host,
        std::wstring& path, int& port)
    {
        std::wstring u = url;
        port = 80;
        if (u.find(L"http://") == 0)  u = u.substr(7);
        else if (u.find(L"https://") == 0) { u = u.substr(8); port = 443; }

        auto sl = u.find(L'/');
        if (sl == std::wstring::npos)
            { host = u; path = L"/"; }
        else
            { host = u.substr(0, sl); path = u.substr(sl); }

        auto col = host.find(L':');
        if (col != std::wstring::npos)
        {
            port = std::stoi(host.substr(col + 1));
            host = host.substr(0, col);
        }
        return true;
    }

    long long DownloadManager::HttpFileSize(
        const std::wstring& h, int p,
        const std::wstring& path)
    {
        auto [hs, hc] = g_ConnPool.Get(h, p);
        if (!hs || !hc) return -1;

        HINTERNET hr = WinHttpOpenRequest(hc, L"HEAD",
            path.c_str(), nullptr, nullptr,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hr) { g_ConnPool.Drop(hs, hc); return -1; }

        if (!WinHttpSendRequest(hr, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hr, nullptr))
        {
            WinHttpCloseHandle(hr);
            g_ConnPool.Drop(hs, hc);
            return -1;
        }

        wchar_t lb[64] = {}; DWORD ls = sizeof(lb); long long fs = -1;
        if (WinHttpQueryHeaders(hr, WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX,
            lb, &ls, WINHTTP_NO_HEADER_INDEX))
            fs = _wtoi64(lb);

        WinHttpCloseHandle(hr);
        g_ConnPool.Put(hs, hc, h, p);
        return fs;
    }

    bool DownloadManager::HttpRangeOk(
        const std::wstring& h, int p,
        const std::wstring& path)
    {
        auto [hs, hc] = g_ConnPool.Get(h, p);
        if (!hs || !hc) return false;

        HINTERNET hr = WinHttpOpenRequest(hc, L"GET",
            path.c_str(), nullptr, nullptr,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hr) { g_ConnPool.Drop(hs, hc); return false; }

        WinHttpAddRequestHeaders(hr, L"Range: bytes=0-0",
            (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(hr, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hr, nullptr))
        {
            WinHttpCloseHandle(hr);
            g_ConnPool.Drop(hs, hc);
            return false;
        }

        DWORD sc = 0, ss = sizeof(sc);
        WinHttpQueryHeaders(hr,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &sc, &ss, WINHTTP_NO_HEADER_INDEX);

        WinHttpCloseHandle(hr);
        g_ConnPool.Put(hs, hc, h, p);
        return sc == 206;
    }

    void DownloadManager::StartTask(
        std::shared_ptr<DownloadTask> t,
        DlUpdateCallback onUpdate,
        DlCompleteCallback onComplete)
    {
        std::thread([this, t, onUpdate, onComplete]()
        {
            ParseUrl(t->url, t->host, t->path, t->port);

            long long fs = HttpFileSize(t->host, t->port, t->path);
            t->fileSize = fs;

            bool rangeOk = (fs > 0) &&
                HttpRangeOk(t->host, t->port, t->path) &&
                (fs >= 1048576);

            if (!rangeOk)
            {
                t->state     = DLState::Downloading;
                t->startTime = GetTickCount();
                t->lastTime  = t->startTime;
                SingleDl(t, onUpdate, onComplete);
                return;
            }

            // Pre-allocate file
            HANDLE hF = CreateFileW(t->savePath.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                nullptr, CREATE_ALWAYS, 0, nullptr);
            if (hF == INVALID_HANDLE_VALUE)
            {
                t->state = DLState::Error;
                onComplete(t->id, DLState::Error);
                return;
            }
            LARGE_INTEGER li; li.QuadPart = fs;
            SetFilePointerEx(hF, li, nullptr, FILE_BEGIN);
            SetEndOfFile(hF);
            CloseHandle(hF);

            // Setup chunks
            int nt = t->nThreads;
            if (fs < (long long)nt * 262144)
                nt = (int)(fs / 262144);
            if (nt < 1) nt = 1;

            long long cs = fs / nt;
            t->chunks.resize(nt);
            for (int i = 0; i < nt; i++)
            {
                t->chunks[i].startByte  = i * cs;
                t->chunks[i].endByte    = (i == nt - 1)
                    ? fs - 1 : (i * cs + cs - 1);
                t->chunks[i].downloaded = 0;
                t->chunks[i].done       = false;
            }

            t->state     = DLState::Downloading;
            t->startTime = GetTickCount();
            t->lastTime  = t->startTime;

            // Launch chunk threads
            std::vector<std::thread> threads;
            for (int i = 0; i < nt; i++)
                threads.emplace_back(
                    [this, t, i]() { ChunkWork(t, i); });
            for (auto& th : threads)
                if (th.joinable()) th.join();

            if (t->cancelled.load())
                t->state = DLState::Cancelled;
            else
            {
                bool ok = true;
                for (auto& c : t->chunks)
                    if (!c.done.load()) { ok = false; break; }
                t->state = ok ? DLState::Complete : DLState::Error;
            }

            onComplete(t->id, t->state.load());
        }).detach();
    }

    void DownloadManager::SingleDl(
        std::shared_ptr<DownloadTask> t,
        DlUpdateCallback onUpdate,
        DlCompleteCallback onComplete)
    {
        HINTERNET hs = WinHttpOpen(L"BDFlix/3",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            nullptr, nullptr, 0);
        if (!hs)
        {
            t->state = DLState::Error;
            onComplete(t->id, DLState::Error);
            return;
        }
        DWORD to = 30000;
        WinHttpSetTimeouts(hs, to, to, to, to);

        HINTERNET hc = WinHttpConnect(hs, t->host.c_str(),
            (INTERNET_PORT)t->port, 0);
        if (!hc)
        {
            WinHttpCloseHandle(hs);
            t->state = DLState::Error;
            onComplete(t->id, DLState::Error);
            return;
        }

        HINTERNET hr = WinHttpOpenRequest(hc, L"GET",
            t->path.c_str(), nullptr, nullptr,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hr)
        {
            WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
            t->state = DLState::Error;
            onComplete(t->id, DLState::Error);
            return;
        }

        if (!WinHttpSendRequest(hr, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hr, nullptr))
        {
            WinHttpCloseHandle(hr);
            WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
            t->state = DLState::Error;
            onComplete(t->id, DLState::Error);
            return;
        }

        // Get Content-Length if not already known
        if (t->fileSize <= 0)
        {
            wchar_t lb[64] = {}; DWORD ls = sizeof(lb);
            if (WinHttpQueryHeaders(hr, WINHTTP_QUERY_CONTENT_LENGTH,
                WINHTTP_HEADER_NAME_BY_INDEX,
                lb, &ls, WINHTTP_NO_HEADER_INDEX))
                t->fileSize = _wtoi64(lb);
        }

        HANDLE hF = CreateFileW(t->savePath.c_str(),
            GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (hF == INVALID_HANDLE_VALUE)
        {
            WinHttpCloseHandle(hr);
            WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
            t->state = DLState::Error;
            onComplete(t->id, DLState::Error);
            return;
        }

        char buf[65536]; DWORD ba = 0;
        while (true)
        {
            if (t->cancelled.load()) break;
            while (t->paused.load() && !t->cancelled.load())
                Sleep(100);
            if (t->cancelled.load()) break;
            if (!WinHttpQueryDataAvailable(hr, &ba) || !ba) break;
            DWORD tr = (ba > sizeof(buf))
                ? (DWORD)sizeof(buf) : ba, ar = 0;
            if (!WinHttpReadData(hr, buf, tr, &ar) || !ar) break;
            DWORD wr;
            WriteFile(hF, buf, ar, &wr, nullptr);
            t->totalDl.fetch_add(wr);

            // Notify UI
            onUpdate(t->id, DLState::Downloading,
                t->totalDl.load(), t->fileSize, t->speed);
        }

        CloseHandle(hF);
        WinHttpCloseHandle(hr);
        WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);

        t->state = t->cancelled.load()
            ? DLState::Cancelled : DLState::Complete;
        onComplete(t->id, t->state.load());
    }

    void DownloadManager::ChunkWork(
        std::shared_ptr<DownloadTask> t, int ci)
    {
        auto& ch = t->chunks[ci];
        long long cur = ch.startByte + ch.downloaded.load();
        if (cur > ch.endByte) { ch.done = true; return; }

        HINTERNET hs = WinHttpOpen(L"BDFlix/3",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            nullptr, nullptr, 0);
        if (!hs) return;
        DWORD to = 30000;
        WinHttpSetTimeouts(hs, to, to, to, to);

        HINTERNET hc = WinHttpConnect(hs, t->host.c_str(),
            (INTERNET_PORT)t->port, 0);
        if (!hc) { WinHttpCloseHandle(hs); return; }

        HINTERNET hr = WinHttpOpenRequest(hc, L"GET",
            t->path.c_str(), nullptr, nullptr,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hr)
        {
            WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
            return;
        }

        std::wstring rh = L"Range: bytes=" +
            std::to_wstring(cur) + L"-" +
            std::to_wstring(ch.endByte);
        WinHttpAddRequestHeaders(hr, rh.c_str(), (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

        if (!WinHttpSendRequest(hr, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hr, nullptr))
        {
            WinHttpCloseHandle(hr);
            WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
            return;
        }

        HANDLE hF = CreateFileW(t->savePath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hF == INVALID_HANDLE_VALUE)
        {
            WinHttpCloseHandle(hr);
            WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
            return;
        }

        LARGE_INTEGER off; off.QuadPart = cur;
        SetFilePointerEx(hF, off, nullptr, FILE_BEGIN);

        char buf[65536]; DWORD ba = 0;
        while (true)
        {
            if (t->cancelled.load()) break;
            while (t->paused.load() && !t->cancelled.load())
                Sleep(100);
            if (t->cancelled.load()) break;
            if (!WinHttpQueryDataAvailable(hr, &ba) || !ba) break;
            DWORD tr = (ba > sizeof(buf))
                ? (DWORD)sizeof(buf) : ba, ar = 0;
            if (!WinHttpReadData(hr, buf, tr, &ar) || !ar) break;
            DWORD wr;
            WriteFile(hF, buf, ar, &wr, nullptr);
            ch.downloaded.fetch_add(wr);
            t->totalDl.fetch_add(wr);
        }

        CloseHandle(hF);
        WinHttpCloseHandle(hr);
        WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);

        if (ch.downloaded.load() >=
            ch.endByte - ch.startByte + 1)
            ch.done = true;
    }
}
