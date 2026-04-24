#pragma once
#include "pch.h"
#include "Models.h"
#include <string>
#include <set>
#include <vector>
#include <sstream>

namespace BDFlix::Helpers
{
    // ── String helpers ────────────────────────────────────────────────────────

    inline std::wstring Lower(const std::wstring& s)
    {
        std::wstring r = s;
        for (auto& c : r) c = towlower(c);
        return r;
    }

    inline std::wstring GetExt(const std::wstring& n)
    {
        auto p = n.rfind(L'.');
        return p == std::wstring::npos ? L"" : Lower(n.substr(p));
    }

    inline std::string W2U(const std::wstring& w)
    {
        if (w.empty()) return "";
        int n = WideCharToMultiByte(CP_UTF8, 0,
            w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string r(n, 0);
        WideCharToMultiByte(CP_UTF8, 0,
            w.c_str(), (int)w.size(), &r[0], n, nullptr, nullptr);
        return r;
    }

    inline std::wstring U2W(const std::string& s)
    {
        if (s.empty()) return L"";
        int n = MultiByteToWideChar(CP_UTF8, 0,
            s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring r(n, 0);
        MultiByteToWideChar(CP_UTF8, 0,
            s.c_str(), (int)s.size(), &r[0], n);
        return r;
    }

    inline std::wstring UrlDec(const std::wstring& s)
    {
        // Percent-encoded URLs encode non-ASCII characters as UTF-8 byte
        // sequences (e.g. %C3%A9 for 'é'). Decode first into a raw byte
        // buffer, then convert the buffer from UTF-8 to wide string so
        // multi-byte sequences are reconstructed correctly.
        std::string bytes;
        bytes.reserve(s.size());
        for (size_t i = 0; i < s.size(); i++)
        {
            wchar_t c = s[i];
            if (c == L'%' && i + 2 < s.size())
            {
                wchar_t hx[3] = { s[i + 1], s[i + 2], 0 };
                wchar_t* e = nullptr;
                long v = wcstol(hx, &e, 16);
                if (e == hx + 2)
                {
                    bytes.push_back(static_cast<char>(static_cast<unsigned char>(v & 0xFF)));
                    i += 2;
                    continue;
                }
            }
            if (c == L'+')
            {
                bytes.push_back(' ');
            }
            else if (c < 0x80)
            {
                bytes.push_back(static_cast<char>(c));
            }
            else
            {
                // Non-ASCII wide char that wasn't percent-encoded; re-encode
                // as UTF-8 so the final U2W call can decode it consistently.
                wchar_t buf[2] = { c, 0 };
                int n = WideCharToMultiByte(CP_UTF8, 0, buf, 1, nullptr, 0, nullptr, nullptr);
                if (n > 0)
                {
                    size_t oldSize = bytes.size();
                    bytes.resize(oldSize + n);
                    WideCharToMultiByte(CP_UTF8, 0, buf, 1, &bytes[oldSize], n, nullptr, nullptr);
                }
            }
        }
        return U2W(bytes);
    }

    inline std::wstring SanitizeFN(const std::wstring& n)
    {
        std::wstring r;
        for (auto c : n)
        {
            if (c == L'\\' || c == L'/' || c == L':' || c == L'*' ||
                c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|')
                r += L'_';
            else r += c;
        }
        return r;
    }

    inline std::wstring GetFolder(const std::wstring& h)
    {
        std::wstring s = h;
        if (!s.empty() && s.back() == L'/') s.pop_back();
        auto p = s.rfind(L'/');
        if (p == std::wstring::npos) return L"Root";
        auto r = s.substr(0, p);
        auto p2 = r.rfind(L'/');
        return p2 == std::wstring::npos ? r : r.substr(p2 + 1);
    }

    inline std::wstring GetName(const std::wstring& h)
    {
        std::wstring s = h;
        if (!s.empty() && s.back() == L'/') s.pop_back();
        auto p = s.rfind(L'/');
        return p == std::wstring::npos ? s : s.substr(p + 1);
    }

    // ── File type checks ──────────────────────────────────────────────────────

    inline bool IsAllowed(const std::wstring& n)
    {
        static const std::set<std::wstring> ok = {
            L".mp3", L".mp4", L".mkv", L".avi", L".mov", L".wmv",
            L".flv", L".webm", L".flac", L".aac", L".ogg", L".wav",
            L".m4a", L".m4v", L".ts", L".iso", L".zip", L".rar",
            L".7z", L".tar", L".gz", L".exe", L".msi", L".apk",
            L".pdf", L".srt", L".sub", L".torrent"
        };
        return ok.count(GetExt(n)) > 0;
    }

    inline bool IsMedia(const std::wstring& n)
    {
        static const std::set<std::wstring> m = {
            L".mp3", L".mp4", L".mkv", L".avi", L".mov", L".flv",
            L".webm", L".flac", L".aac", L".ogg", L".wav", L".m4a",
            L".m4v", L".ts"
        };
        return m.count(GetExt(n)) > 0;
    }

    inline bool IsVideo(const std::wstring& n)
    {
        static const std::set<std::wstring> v = {
            L".mp4", L".mkv", L".avi", L".mov", L".flv",
            L".webm", L".m4v", L".ts", L".wmv"
        };
        return v.count(GetExt(n)) > 0;
    }

    inline bool IsAudio(const std::wstring& n)
    {
        static const std::set<std::wstring> a = {
            L".mp3", L".flac", L".aac", L".ogg", L".wav", L".m4a"
        };
        return a.count(GetExt(n)) > 0;
    }

    inline bool IsArchive(const std::wstring& n)
    {
        static const std::set<std::wstring> a = {
            L".zip", L".rar", L".7z", L".tar", L".gz", L".iso"
        };
        return a.count(GetExt(n)) > 0;
    }

    inline bool IsExe(const std::wstring& n)
    {
        static const std::set<std::wstring> e = { L".exe", L".msi", L".apk" };
        return e.count(GetExt(n)) > 0;
    }

    inline bool IsDoc(const std::wstring& n)
    {
        static const std::set<std::wstring> d = {
            L".pdf", L".srt", L".sub", L".torrent"
        };
        return d.count(GetExt(n)) > 0;
    }

    inline winrt::Microsoft::UI::Xaml::Media::SolidColorBrush
        GetFileTypeBrush(const std::wstring& name,
            winrt::Microsoft::UI::Xaml::ResourceDictionary const& res)
    {
        std::wstring key;
        if (IsVideo(name))        key = L"VideoBrush";
        else if (IsAudio(name))   key = L"AudioBrush";
        else if (IsArchive(name)) key = L"ArchiveBrush";
        else if (IsExe(name))     key = L"ExecutableBrush";
        else if (IsDoc(name))     key = L"DocumentBrush";
        else                       key = L"DefaultFTBrush";

        return res.Lookup(winrt::box_value(key)).as<
            winrt::Microsoft::UI::Xaml::Media::SolidColorBrush>();
    }

    // ── Formatting ────────────────────────────────────────────────────────────

    inline std::wstring FmtSize(long long b)
    {
        if (b < 0) return L"\u2014";
        wchar_t buf[64];
        if (b < 1024)            { swprintf(buf, 64, L"%lld B", b); }
        else if (b < 1048576)    { swprintf(buf, 64, L"%.1f KB", b / 1024.0); }
        else if (b < 1073741824LL) { swprintf(buf, 64, L"%.1f MB", b / 1048576.0); }
        else                     { swprintf(buf, 64, L"%.2f GB", b / 1073741824.0); }
        return buf;
    }

    inline std::wstring FmtSpeed(double s)
    {
        if (s <= 0) return L"0 B/s";
        wchar_t buf[64];
        if (s < 1024)         { swprintf(buf, 64, L"%.0f B/s", s); }
        else if (s < 1048576) { swprintf(buf, 64, L"%.1f KB/s", s / 1024.0); }
        else                  { swprintf(buf, 64, L"%.1f MB/s", s / 1048576.0); }
        return buf;
    }

    inline std::wstring FmtETA(long long rem, double spd)
    {
        if (spd <= 0 || rem <= 0) return L"\u2014";
        long long s = (long long)(rem / spd);
        wchar_t buf[32];
        if (s < 60)        swprintf(buf, 32, L"%llds", s);
        else if (s < 3600) swprintf(buf, 32, L"%lldm %llds", s / 60, s % 60);
        else               swprintf(buf, 32, L"%lldh %lldm", s / 3600, (s % 3600) / 60);
        return buf;
    }

    inline long long ParseSize(const std::wstring& s)
    {
        if (s.empty()) return -1;
        try
        {
            size_t i = 0;
            while (i < s.size() && (iswdigit(s[i]) || s[i] == L'.')) i++;
            if (!i) return -1;
            double v = std::stod(s.substr(0, i));
            std::wstring u = Lower(s.substr(i));
            while (!u.empty() && u[0] == L' ') u.erase(0, 1);
            if (u.find(L"tb") != std::wstring::npos) return (long long)(v * 1099511627776.0);
            if (u.find(L"gb") != std::wstring::npos) return (long long)(v * 1073741824.0);
            if (u.find(L"mb") != std::wstring::npos) return (long long)(v * 1048576.0);
            if (u.find(L"kb") != std::wstring::npos) return (long long)(v * 1024.0);
            return (long long)v;
        }
        catch (...) { return -1; }
    }

    // ── Search tokenizer ──────────────────────────────────────────────────────

    struct Tok { std::wstring t; bool neg; };

    inline std::vector<Tok> Tokenize(const std::wstring& q)
    {
        std::vector<Tok> toks;
        size_t i = 0;
        while (i < q.size())
        {
            while (i < q.size() && q[i] == L' ') i++;
            if (i >= q.size()) break;
            Tok tk; tk.neg = false;
            if (q[i] == L'-') { tk.neg = true; i++; }
            if (i < q.size() && q[i] == L'"')
            {
                i++;
                size_t s = i;
                while (i < q.size() && q[i] != L'"') i++;
                tk.t = Lower(q.substr(s, i - s));
                if (i < q.size()) i++;
            }
            else
            {
                size_t s = i;
                while (i < q.size() && q[i] != L' ') i++;
                tk.t = Lower(q.substr(s, i - s));
            }
            if (!tk.t.empty()) toks.push_back(tk);
        }
        return toks;
    }

    inline bool Match(const std::wstring& name, const std::vector<Tok>& toks)
    {
        std::wstring lo = Lower(name);
        for (auto& t : toks)
        {
            bool f = lo.find(t.t) != std::wstring::npos;
            if (t.neg && f)  return false;
            if (!t.neg && !f) return false;
        }
        return true;
    }

    // ── JSON mini-parser ──────────────────────────────────────────────────────

    inline std::wstring JStr(const std::wstring& j, size_t sp,
        const std::wstring& k)
    {
        std::wstring sk = L"\"" + k + L"\"";
        size_t p = j.find(sk, sp);
        if (p == std::wstring::npos) return L"";
        p += sk.size();
        while (p < j.size() &&
            (j[p] == L' ' || j[p] == L':' || j[p] == L'\t' ||
                j[p] == L'\n' || j[p] == L'\r')) p++;
        if (p >= j.size() || j[p] != L'"') return L"";
        p++;
        std::wstring r;
        while (p < j.size() && j[p] != L'"')
        {
            if (j[p] == L'\\' && p + 1 < j.size())
            {
                p++;
                if (j[p] == L'"')  r += L'"';
                else if (j[p] == L'\\') r += L'\\';
                else if (j[p] == L'/')  r += L'/';
                else if (j[p] == L'n')  r += L'\n';
                else if (j[p] == L'u' && p + 4 < j.size())
                {
                    wchar_t hx[5] = { j[p+1], j[p+2], j[p+3], j[p+4], 0 };
                    r += (wchar_t)wcstol(hx, nullptr, 16);
                    p += 4;
                }
                else r += j[p];
            }
            else r += j[p];
            p++;
        }
        return r;
    }

    inline std::wstring JNum(const std::wstring& j, size_t sp,
        const std::wstring& k)
    {
        std::wstring sk = L"\"" + k + L"\"";
        size_t p = j.find(sk, sp);
        if (p == std::wstring::npos) return L"";
        p += sk.size();
        while (p < j.size() &&
            (j[p] == L' ' || j[p] == L':' || j[p] == L'\t' ||
                j[p] == L'\n' || j[p] == L'\r')) p++;
        if (p >= j.size()) return L"";
        std::wstring r;
        while (p < j.size() && (iswdigit(j[p]) || j[p] == L'.'))
            r += j[p++];
        return r;
    }

    struct JItem { std::wstring href, size; };

    inline std::vector<JItem> ParseJ(const std::wstring& j)
    {
        std::vector<JItem> items;
        size_t p = 0;
        while (true)
        {
            size_t a = j.find(L'{', p);
            if (a == std::wstring::npos) break;
            size_t b = j.find(L'}', a);
            if (b == std::wstring::npos) break;
            std::wstring o = j.substr(a, b - a + 1);
            std::wstring h = JStr(o, 0, L"href");
            std::wstring s = JNum(o, 0, L"size");
            if (s.empty()) s = JStr(o, 0, L"size");
            if (!h.empty()) items.push_back({ h, s });
            p = b + 1;
        }
        return items;
    }

    // ── Download folder ───────────────────────────────────────────────────────

    inline std::wstring DlFolder()
    {
        wchar_t* p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p)))
        {
            std::wstring r = p;
            CoTaskMemFree(p);
            return r;
        }
        wchar_t b[MAX_PATH];
        GetEnvironmentVariableW(L"USERPROFILE", b, MAX_PATH);
        return std::wstring(b) + L"\\Downloads";
    }

    // ── IDM finder ────────────────────────────────────────────────────────────

    inline std::wstring FindIDM()
    {
        const wchar_t* ps[] = {
            L"C:\\Program Files (x86)\\Internet Download Manager\\IDMan.exe",
            L"C:\\Program Files\\Internet Download Manager\\IDMan.exe"
        };
        for (auto p : ps)
            if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) return p;

        HKEY hk;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\DownloadManager",
            0, KEY_READ, &hk) == ERROR_SUCCESS)
        {
            wchar_t ep[MAX_PATH] = {};
            DWORD sz = sizeof(ep);
            if (RegQueryValueExW(hk, L"ExePath", nullptr, nullptr,
                (LPBYTE)ep, &sz) == ERROR_SUCCESS)
            {
                RegCloseKey(hk);
                if (GetFileAttributesW(ep) != INVALID_FILE_ATTRIBUTES) return ep;
            }
            else RegCloseKey(hk);
        }
        wchar_t sp[MAX_PATH] = {};
        if (SearchPathW(nullptr, L"IDMan.exe", nullptr, MAX_PATH, sp, nullptr))
            return sp;
        return L"";
    }
}
