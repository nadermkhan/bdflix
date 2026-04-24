#include "pch.h"
#include "SearchPage.xaml.h"
#if __has_include("SearchPage.g.cpp")
#include "SearchPage.g.cpp"
#endif

#include "DownloadsPage.xaml.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.System.h>
#include <algorithm>
#include <fstream>

namespace winrt::BDFlix::implementation
{
    // ── FileResultVM::TypeColor ──────────────────────────────────────────────
    Microsoft::UI::Xaml::Media::SolidColorBrush
        FileResultVM::TypeColor() const
    {
        std::wstring key;
        if (::BDFlix::Helpers::IsVideo(data.name))       key = L"VideoBrush";
        else if (::BDFlix::Helpers::IsAudio(data.name))  key = L"AudioBrush";
        else if (::BDFlix::Helpers::IsArchive(data.name))key = L"ArchiveBrush";
        else if (::BDFlix::Helpers::IsExe(data.name))    key = L"ExecutableBrush";
        else if (::BDFlix::Helpers::IsDoc(data.name))    key = L"DocumentBrush";
        else                                               key = L"DefaultFTBrush";

        return Application::Current().Resources()
            .Lookup(box_value(key))
            .as<Microsoft::UI::Xaml::Media::SolidColorBrush>();
    }

    // ── SearchPage ───────────────────────────────────────────────────────────
    SearchPage::SearchPage()
    {
        InitializeComponent();

        m_dispatcher =
            Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        // Create observable collection bound to ListView
        m_items = winrt::single_threaded_observable_vector<
            winrt::Windows::Foundation::IInspectable>();
        ResultsList().ItemsSource(m_items);
    }

    void SearchPage::SearchBox_KeyDown(
        IInspectable const&,
        Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            auto text = winrt::to_string(SearchBox().Text());
            std::wstring term = winrt::to_hstring(text).c_str();
            // trim
            while (!term.empty() && term.front() == L' ')
                term.erase(0, 1);
            while (!term.empty() && term.back() == L' ')
                term.pop_back();
            if (term.size() >= 2)
                StartSearch(term);
        }
    }

    void SearchPage::SearchButton_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto term = std::wstring(SearchBox().Text().c_str());
        while (!term.empty() && term.front() == L' ') term.erase(0, 1);
        while (!term.empty() && term.back() == L' ')  term.pop_back();
        if (term.size() >= 2) StartSearch(term);
    }

    void SearchPage::StartSearch(const std::wstring& term)
    {
        // UI feedback
        EmptyState().Text(L"Searching...");
        SearchProgress().Visibility(
            Microsoft::UI::Xaml::Visibility::Visible);
        SearchProgress().IsActive(true);
        ResultCount().Text(L"");
        m_items.Clear();

        {
            std::lock_guard<std::mutex> lk(m_dataMutex);
            m_allResults.clear();
        }

        // Batch callback — called from background thread
        auto onBatch = [this](std::vector<::BDFlix::FileResult> batch)
        {
            {
                std::lock_guard<std::mutex> lk(m_dataMutex);
                m_allResults.insert(m_allResults.end(),
                    batch.begin(), batch.end());
            }

            // Marshal to UI thread
            m_dispatcher.TryEnqueue([this, batch = std::move(batch)]()
            {
                for (auto& f : batch)
                {
                    m_items.Append(winrt::make<FileResultVM>(f));
                }
                auto n = m_items.Size();
                ResultCount().Text(winrt::hstring(
                    std::to_wstring(n) + L" files"));
                EmptyState().Visibility(n == 0
                    ? Microsoft::UI::Xaml::Visibility::Visible
                    : Microsoft::UI::Xaml::Visibility::Collapsed);
            });
        };

        // Complete callback
        auto onComplete = [this](int total)
        {
            m_dispatcher.TryEnqueue([this, total]()
            {
                SearchProgress().IsActive(false);
                SearchProgress().Visibility(
                    Microsoft::UI::Xaml::Visibility::Collapsed);

                auto n = m_items.Size();
                ResultCount().Text(winrt::hstring(
                    std::to_wstring(n) + L" files found"));

                if (n == 0)
                    EmptyState().Text(L"No results found");
                else
                {
                    EmptyState().Visibility(
                        Microsoft::UI::Xaml::Visibility::Collapsed);
                    if (m_sortCol >= 0) ApplySort();
                }
            });
        };

        m_engine.Search(term, onBatch, onComplete);
    }

    void SearchPage::ApplySort()
    {
        std::lock_guard<std::mutex> lk(m_dataMutex);
        int c = m_sortCol; bool a = m_sortAsc;

        std::sort(m_allResults.begin(), m_allResults.end(),
            [c, a](const ::BDFlix::FileResult& x,
                const ::BDFlix::FileResult& y)
        {
            int r = 0;
            switch (c)
            {
            case 0: r = _wcsicmp(x.name.c_str(), y.name.c_str()); break;
            case 1: r = (x.sizeBytes < y.sizeBytes) ? -1
                      : (x.sizeBytes > y.sizeBytes) ? 1 : 0;       break;
            case 2: r = _wcsicmp(x.server.c_str(), y.server.c_str()); break;
            }
            return a ? r < 0 : r > 0;
        });

        // Rebuild items
        m_items.Clear();
        for (auto& f : m_allResults)
        {
            m_items.Append(winrt::make<FileResultVM>(f));
        }
    }

    void SearchPage::SortName_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_sortCol == 0) m_sortAsc = !m_sortAsc;
        else { m_sortCol = 0; m_sortAsc = true; }
        ApplySort();
    }

    void SearchPage::SortSize_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_sortCol == 1) m_sortAsc = !m_sortAsc;
        else { m_sortCol = 1; m_sortAsc = false; }
        ApplySort();
    }

    void SearchPage::SortServer_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_sortCol == 2) m_sortAsc = !m_sortAsc;
        else { m_sortCol = 2; m_sortAsc = true; }
        ApplySort();
    }

    void SearchPage::ResultsList_DoubleTapped(
        IInspectable const&,
        Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&)
    {
        DownloadSelected();
    }

    void SearchPage::ResultsList_RightTapped(
        IInspectable const&,
        Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
    {
        // If nothing selected, select the item under cursor
        auto elem = e.OriginalSource().as<
            Microsoft::UI::Xaml::FrameworkElement>();
        if (elem)
        {
            // Walk up to find ListViewItem
            auto parent = elem;
            while (parent)
            {
                if (auto lvi = parent.try_as<
                    Microsoft::UI::Xaml::Controls::ListViewItem>())
                {
                    ResultsList().SelectedItem(lvi.Content());
                    break;
                }
                parent = parent.Parent().try_as<
                    Microsoft::UI::Xaml::FrameworkElement>();
            }
        }

        FileContextMenu().ShowAt(
            ResultsList(),
            e.GetPosition(ResultsList()));
    }

    void SearchPage::DownloadSelected()
    {
        auto selected = ResultsList().SelectedItems();
        if (selected.Size() == 0) return;

        for (auto item : selected)
        {
            auto vm = item.as<FileResultVM>();
            if (!vm) continue;
            auto& f = vm->data;

            ::BDFlix::g_DlManager.QueueDownload(
                f.fullUrl, f.name,
                // onUpdate — we don't need per-file updates here
                [](int, ::BDFlix::DLState,
                    long long, long long, double) {},
                // onComplete
                [](int, ::BDFlix::DLState) {}
            );
        }
    }

    void SearchPage::DlIDM(const std::wstring& url)
    {
        std::wstring p = ::BDFlix::Helpers::FindIDM();
        if (p.empty())
        {
            // Show content dialog (simplified)
            return;
        }
        ShellExecuteW(nullptr, L"open",
            p.c_str(),
            (L"/d \"" + url + L"\" /n").c_str(),
            nullptr, SW_SHOWNORMAL);
    }

    void SearchPage::MakePlaylist(
        const std::wstring& folder,
        const std::vector<::BDFlix::FileResult>& files)
    {
        std::wstring c = L"#EXTM3U\r\n";
        int n = 0;
        for (auto& f : files)
        {
            if (::BDFlix::Helpers::IsMedia(f.name))
            {
                c += L"#EXTINF:-1," + f.name + L"\r\n" +
                     f.fullUrl + L"\r\n";
                n++;
            }
        }
        if (!n) return;

        std::wstring fp = ::BDFlix::Helpers::DlFolder() + L"\\" +
            ::BDFlix::Helpers::SanitizeFN(folder) + L"_" +
            std::to_wstring(time(nullptr)) + L".m3u8";

        std::string u = ::BDFlix::Helpers::W2U(c);
        HANDLE h = CreateFileW(fp.c_str(), GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD w;
        unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        WriteFile(h, bom, 3, &w, nullptr);
        WriteFile(h, u.c_str(), (DWORD)u.size(), &w, nullptr);
        CloseHandle(h);
    }

    // ── Context menu handlers ────────────────────────────────────────────────

    void SearchPage::CtxDownload_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        DownloadSelected();
    }

    void SearchPage::CtxIDM_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        for (auto item : ResultsList().SelectedItems())
        {
            auto vm = item.as<FileResultVM>();
            if (vm) DlIDM(vm->data.fullUrl);
        }
    }

    void SearchPage::CtxCopyUrl_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        std::wstring txt;
        for (auto item : ResultsList().SelectedItems())
        {
            auto vm = item.as<FileResultVM>();
            if (vm)
            {
                if (!txt.empty()) txt += L"\r\n";
                txt += vm->data.fullUrl;
            }
        }
        if (!txt.empty())
        {
            Windows::ApplicationModel::DataTransfer::DataPackage dp;
            dp.SetText(winrt::hstring(txt));
            Windows::ApplicationModel::DataTransfer::
                Clipboard::SetContent(dp);
        }
    }

    void SearchPage::CtxCopyName_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        std::wstring txt;
        for (auto item : ResultsList().SelectedItems())
        {
            auto vm = item.as<FileResultVM>();
            if (vm)
            {
                if (!txt.empty()) txt += L"\r\n";
                txt += vm->data.name;
            }
        }
        if (!txt.empty())
        {
            Windows::ApplicationModel::DataTransfer::DataPackage dp;
            dp.SetText(winrt::hstring(txt));
            Windows::ApplicationModel::DataTransfer::
                Clipboard::SetContent(dp);
        }
    }

    void SearchPage::CtxPlaylist_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto selected = ResultsList().SelectedItem();
        if (!selected) return;
        auto vm = selected.as<FileResultVM>();
        if (!vm) return;

        std::lock_guard<std::mutex> lk(m_dataMutex);
        std::vector<::BDFlix::FileResult> ff;
        for (auto& x : m_allResults)
        {
            if (x.folder == vm->data.folder &&
                x.server == vm->data.server)
                ff.push_back(x);
        }
        MakePlaylist(vm->data.folder, ff);
    }
}
