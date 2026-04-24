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
        Cancel(); // mark any previous context cancelled and wait briefly

        auto ctx = std::make_shared<Ctx>();
        ctx->total = (int)g_Servers.size();

        {
            std::lock_guard<std::mutex> lk(m_ctxMutex);
            m_ctx = ctx;
        }
        m_searching = true;

        auto toks = Helpers::Tokenize(term);

        for (auto& srv : g_Servers)
        {
            std::thread([this, ctx, srv, term, toks,
                onBatch, onComplete]()
            {
                SearchServer(ctx, srv, term, toks, onBatch, onComplete);
            }).detach();
        }
    }

    void SearchEngine::Cancel()
    {
        std::shared_ptr<Ctx> ctx;
        {
            std::lock_guard<std::mutex> lk(m_ctxMutex);
            ctx = m_ctx;
        }
        if (ctx) ctx->cancelled = true;

        // Wait briefly for threads to notice cancellation before returning.
        // Threads that miss this window will still run to completion but will
        // only mutate their own (now-cancelled) context, never the next one.
        int waited = 0;
        while (m_searching.load() && waited < 500)
        {
            Sleep(50);
            waited += 50;
        }
        m_searching = false;
    }

    void SearchEngine::SearchServer(
        std::shared_ptr<Ctx> ctx,
        const ServerInfo& srv,
        const std::wstring& term,
        const std::vector<Helpers::Tok>& toks,
        ResultBatchCallback onBatch,
        SearchCompleteCallback onComplete)
    {
        auto finish = [this, ctx, onComplete]()
        {
            int done = ++ctx->done;
            if (done >= ctx->total)
            {
                // Only the current in-flight context should flip the
                // engine-level searching flag; stale contexts simply exit.
                {
                    std::lock_guard<std::mutex> lk(m_ctxMutex);
                    if (m_ctx == ctx) m_searching = false;
                }
                if (!ctx->cancelled.load())
                {
                    std::lock_guard<std::mutex> lk(ctx->resultsMutex);
                    onComplete((int)ctx->results.size());
                }
            }
        };

        if (ctx->cancelled.load())
        {
            finish();
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

        if (ok && !resp.empty() && !ctx->cancelled.load())
        {
            auto items = Helpers::ParseJ(Helpers::U2W(resp));
            std::vector<FileResult> batch;

            for (auto& it : items)
            {
                if (ctx->cancelled.load()) break;
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

            if (!batch.empty() && !ctx->cancelled.load())
            {
                {
                    std::lock_guard<std::mutex> lk(ctx->resultsMutex);
                    ctx->results.insert(ctx->results.end(),
                        batch.begin(), batch.end());
                }
                onBatch(std::move(batch));
            }
        }

        finish();
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
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            (port == 443) ? WINHTTP_FLAG_SECURE : 0);
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

        char buf[8192];
        DWORD avail = 0, read = 0;
        while (WinHttpQueryDataAvailable(hr, &avail) && avail)
        {
            DWORD toRead = avail > sizeof(buf) ? (DWORD)sizeof(buf) : avail;
            if (!WinHttpReadData(hr, buf, toRead, &read) || !read) break;
            resp.append(buf, read);
        }

        WinHttpCloseHandle(hr);
        g_ConnPool.Put(hs, hc, host, port);
        ok = true;
        return resp;
    }
}
