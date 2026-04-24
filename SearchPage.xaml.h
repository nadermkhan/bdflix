#pragma once
#include "SearchPage.g.h"
#include "Models.h"
#include "SearchEngine.h"
#include "DownloadManager.h"
#include "Helpers.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::BDFlix::implementation
{
    // ── Observable view-model for a single file result ──────────────────────
    struct FileResultVM :
        winrt::implements<FileResultVM,
            winrt::Windows::Foundation::IInspectable,
            winrt::Windows::Foundation::IStringable>
    {
        ::BDFlix::FileResult data;

        winrt::hstring Name()     const { return winrt::hstring(data.name); }
        winrt::hstring Server()   const { return winrt::hstring(data.server); }
        winrt::hstring Folder()   const { return winrt::hstring(data.folder); }
        winrt::hstring SizeText() const
        {
            return winrt::hstring(
                ::BDFlix::Helpers::FmtSize(data.sizeBytes));
        }
        winrt::hstring ExtLabel() const
        {
            std::wstring e = data.ext;
            if (!e.empty() && e[0] == L'.') e = e.substr(1);
            for (auto& c : e) c = towupper(c);
            if (e.size() > 4) e = e.substr(0, 4);
            return winrt::hstring(e);
        }
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush
            TypeColor() const;  // implemented in .cpp

        winrt::hstring ToString() const { return Name(); }
    };

    // ── Page ────────────────────────────────────────────────────────────────
    struct SearchPage : SearchPageT<SearchPage>
    {
        SearchPage();

        // XAML event handlers
        void SearchBox_KeyDown(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);

        void SearchButton_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void ResultsList_DoubleTapped(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&);

        void ResultsList_RightTapped(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const&);

        void SortName_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void SortSize_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void SortServer_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        // Context menu handlers
        void CtxDownload_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxIDM_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxCopyUrl_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxCopyName_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxPlaylist_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void StartSearch(const std::wstring& term);
        void ApplySort();
        void DownloadSelected();
        void DlIDM(const std::wstring& url);
        void MakePlaylist(const std::wstring& folder,
            const std::vector<::BDFlix::FileResult>& files);

        winrt::Microsoft::UI::Dispatching::DispatcherQueue
            m_dispatcher{ nullptr };

        // Full results + sorted display
        std::mutex m_dataMutex;
        std::vector<::BDFlix::FileResult> m_allResults;

        winrt::Windows::Foundation::Collections::
            IObservableVector<winrt::Windows::Foundation::IInspectable>
            m_items{ nullptr };

        ::BDFlix::SearchEngine m_engine;

        int  m_sortCol{ -1 };
        bool m_sortAsc{ true };
    };
}

namespace winrt::BDFlix::factory_implementation
{
    struct SearchPage :
        SearchPageT<SearchPage, implementation::SearchPage> {};
}
