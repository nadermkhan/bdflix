#include "pch.h"
#include "SearchEngine.h"
#include <thread>

namespace BDFlix
{
    void SearchEngine::Search(
        const std::wstring& term,
        ResultBatchCallback onBatch,
        SearchCompleteCallback onComplete)
    {
        Cancel(); // cancel any previous
        m_cancelled = false;
        m_searching = true;
        m_done = 0;
        m_total = (int)g_Servers.size();
        {
            std::lock_guard<std::mutex> lk(m_resultsMutex);
            m_allResults.clear();
        }

        auto toks = Helpers::Tokenize(term);

        for (auto& srv : g_Servers)
        {
            std::thread([this, srv, term, toks,
                onBatch, onComplete]()
            {
                SearchServer(srv, term, toks, onBatch, onComplete);
            }).detach();
        }
    }

    void SearchEngine::Cancel()
    {
        m_cancelled = true;
        // Wait briefly for threads to notice
        int waited = 0;
        while (m_searching.load() && waited < 500)
        {
            Sleep(50);
            waited += 50;
        }
        m_searching = false;
    }

    void SearchEngine::SearchServer(
        const ServerInfo& srv,
        const std::wstring& term,
        const std::vector<Helpers::Tok>& toks,
        ResultBatchCallback onBatch,
        SearchCompleteCallback onComplete)
    {
        if (m_cancelled.load())
        {
            if (++m_done >= m_total)
            {
                m_searching = false;
                std::lock_guard<std::mutex> lk(m_resultsMutex);
                onComplete((int)m_allResults.size());
            }
            return;
        }

        // Find first non-negative token as pattern
        std::wstring pat = term;
        for (auto& t : toks) if (!t.neg) { pat = t.t; break; }

        std::string body =
            "{\"action\":\"get\",\"search\":{\"href\":\"/" +
            Helpers::W2U(srv.name) +
            "/\",\"pattern\":\"" + Helpers::W2U(pat) +
            "\",\"ignorecase\":true}}";

        bool ok = false;
        std::string resp = HttpPost(srv.host, srv.port,
            srv.path, body, ok);

        if (ok && !resp.empty() && !m_cancelled.load())
        {
            auto items = Helpers::ParseJ(Helpers::U2W(resp));
            std::vector<FileResult> batch;

            for (auto& it : items)
            {
                if (m_cancelled.load()) break;
                std::wstring dn = Helpers::UrlDec(
                    Helpers::GetName(it.href));
                if (!Helpers::IsAllowed(dn) ||
                    !Helpers::Match(dn, toks)) continue;

                FileResult f;
                f.name     = dn;
                f.href     = it.href;
                f.fullUrl  = L"http://" + srv.host + it.href;
                f.size     = it.size;
                f.sizeBytes = Helpers::ParseSize(it.size);
                f.server   = srv.name;
                f.folder   = Helpers::UrlDec(
                    Helpers::GetFolder(it.href));
                f.ext      = Helpers::GetExt(dn);
                batch.push_back(std::move(f));
            }

            if (!batch.empty() && !m_cancelled.load())
            {
                {
                    std::lock_guard<std::mutex> lk(m_resultsMutex);
                    m_allResults.insert(m_allResults.end(),
                        batch.begin(), batch.end());
                }
                onBatch(std::move(batch));
            }
        }

        int done = ++m_done;
        if (done >= m_total)
        {
            m_searching = false;
            std::lock_guard<std::mutex> lk(m_resultsMutex);
            onComplete((int)m_allResults.size());
        }
    }

    std::string SearchEngine::HttpPost(
        const std::wstring& host, int port,
        const std::wstring& path,
        const std::string& body, bool& ok)
    {
        ok = false;
        std::string resp;
        auto [hs, hc] = g_ConnPool.Get(host, port);
        if (!hs || !hc) return "";

        HINTERNET hr = WinHttpOpenRequest(hc, L"POST",
            path.c_str(), nullptr, nullptr,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hr) { g_ConnPool.Drop(hs, hc); return ""; }

        WinHttpAddRequestHeaders(hr,
            L"Connection: Keep-Alive\r\n", (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(hr,
            L"Content-Type: application/json\r\n", (DWORD)-1,
            (void*)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0))
        {
            WinHttpCloseHandle(hr);
            g_ConnPool.Drop(hs, hc);
            return "";
        }

        if (!WinHttpReceiveResponse(hr, nullptr))
        {
            WinHttpCloseHandle(hr);
            g_ConnPool.Drop(hs, hc);
            return "";
        }

        DWORD sc = 0, ss = sizeof(sc);
        WinHttpQueryHeaders(hr,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &sc, &ss, WINHTTP_NO_HEADER_INDEX);

        if (sc != 200)
        {
            WinHttpCloseHandle(hr);
            g_ConnPool.Drop(hs, hc);
            return "";
        }

        DWORD dw = 0, dr = 0;
        do
        {
            if (!WinHttpQueryDataAvailable(hr, &dw) || !dw) break;
            std::vector<char> buf(dw + 1, 0);
            if (WinHttpReadData(hr, buf.data(), dw, &dr))
                resp.append(buf.data(), dr);
        } while (dw > 0);

        WinHttpCloseHandle(hr);
        g_ConnPool.Put(hs, hc, host, port);
        ok = true;
        return resp;
    }
}
