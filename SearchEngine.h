#pragma once
#include "pch.h"
#include "Models.h"
#include "Helpers.h"
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <winhttp.h>

namespace BDFlix
{
    class ConnPool
    {
        struct Entry
        {
            HINTERNET session;
            HINTERNET connect;
            std::wstring host;
            int port;
            DWORD time;
        };
        std::mutex m_mutex;
        std::vector<Entry> m_entries;

    public:
        ~ConnPool() { Flush(); }

        void Flush()
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            for (auto& e : m_entries)
            {
                if (e.connect) WinHttpCloseHandle(e.connect);
                if (e.session) WinHttpCloseHandle(e.session);
            }
            m_entries.clear();
        }

        std::pair<HINTERNET, HINTERNET> Get(
            const std::wstring& h, int p)
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            DWORD now = GetTickCount();
            for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
            {
                if (it->host == h && it->port == p)
                {
                    if (now - it->time < 60000)
                    {
                        auto r = std::make_pair(it->session, it->connect);
                        m_entries.erase(it);
                        return r;
                    }
                    if (it->connect) WinHttpCloseHandle(it->connect);
                    if (it->session) WinHttpCloseHandle(it->session);
                    m_entries.erase(it);
                    break;
                }
            }
            HINTERNET s = WinHttpOpen(L"BDFlix/3",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
            if (!s) return { nullptr, nullptr };
            DWORD to = 25000;
            WinHttpSetTimeouts(s, to, to, to, to);
            HINTERNET c = WinHttpConnect(s, h.c_str(),
                (INTERNET_PORT)p, 0);
            if (!c) { WinHttpCloseHandle(s); return { nullptr, nullptr }; }
            return { s, c };
        }

        void Put(HINTERNET s, HINTERNET c,
            const std::wstring& h, int p)
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            while (m_entries.size() >= 16)
            {
                auto& o = m_entries.front();
                if (o.connect) WinHttpCloseHandle(o.connect);
                if (o.session) WinHttpCloseHandle(o.session);
                m_entries.erase(m_entries.begin());
            }
            m_entries.push_back({ s, c, h, p, GetTickCount() });
        }

        void Drop(HINTERNET s, HINTERNET c)
        {
            if (c) WinHttpCloseHandle(c);
            if (s) WinHttpCloseHandle(s);
        }
    };

    // Global connection pool shared between search & downloads
    inline ConnPool g_ConnPool;

    // Global server list
    inline std::vector<ServerInfo> g_Servers = {
        { L"DHAKA-FLIX-7",  L"172.16.50.7",  L"/DHAKA-FLIX-7/",  80 },
        { L"DHAKA-FLIX-8",  L"172.16.50.8",  L"/DHAKA-FLIX-8/",  80 },
        { L"DHAKA-FLIX-9",  L"172.16.50.9",  L"/DHAKA-FLIX-9/",  80 },
        { L"DHAKA-FLIX-12", L"172.16.50.12", L"/DHAKA-FLIX-12/", 80 },
        { L"DHAKA-FLIX-14", L"172.16.50.14", L"/DHAKA-FLIX-14/", 80 }
    };

    // Callbacks
    using ResultBatchCallback =
        std::function<void(std::vector<FileResult>)>;
    using SearchCompleteCallback =
        std::function<void(int totalFound)>;

    class SearchEngine
    {
    public:
        // Per-search context. Each Search() call creates a fresh context so
        // threads from prior (cancelled) searches can finish without mutating
        // state belonging to a newer search.
        struct Ctx
        {
            std::atomic<bool> cancelled{ false };
            std::atomic<int>  done{ 0 };
            int               total{ 0 };
            std::mutex        resultsMutex;
            std::vector<FileResult> results;
        };

        SearchEngine() = default;
        ~SearchEngine() { Cancel(); }

        void Search(const std::wstring& term,
            ResultBatchCallback onBatch,
            SearchCompleteCallback onComplete);
        void Cancel();

        bool IsSearching() const { return m_searching.load(); }

    private:
        void SearchServer(std::shared_ptr<Ctx> ctx,
            const ServerInfo& srv,
            const std::wstring& term,
            const std::vector<Helpers::Tok>& toks,
            ResultBatchCallback onBatch,
            SearchCompleteCallback onComplete);

        std::string HttpPost(const std::wstring& host, int port,
            const std::wstring& path,
            const std::string& body, bool& ok);

        std::atomic<bool>  m_searching{ false };
        std::mutex         m_ctxMutex;
        std::shared_ptr<Ctx> m_ctx;
    };
}
