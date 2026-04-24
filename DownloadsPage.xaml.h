#pragma once
#include "DownloadsPage.g.h"
#include "Models.h"
#include "DownloadManager.h"
#include "Helpers.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::BDFlix::implementation
{
    // ── Observable VM for one download ──────────────────────────────────────
    struct DlItemVM :
        winrt::implements<DlItemVM,
            winrt::Windows::Foundation::IInspectable>
    {
        std::shared_ptr<::BDFlix::DownloadTask> task;

        explicit DlItemVM(
            std::shared_ptr<::BDFlix::DownloadTask> t)
            : task(std::move(t)) {}

        winrt::hstring FileName() const
        { return winrt::hstring(task->fileName); }

        double Progress() const
        {
            if (task->fileSize <= 0) return 0.0;
            return (double)task->totalDl.load() /
                task->fileSize * 100.0;
        }

        bool ShowProgressRaw() const
        {
            auto st = task->state.load();
            return st == ::BDFlix::DLState::Downloading ||
                   st == ::BDFlix::DLState::Paused;
        }

        winrt::Microsoft::UI::Xaml::Visibility ShowProgress() const
        {
            return ShowProgressRaw()
                ? winrt::Microsoft::UI::Xaml::Visibility::Visible
                : winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::hstring StateIcon() const
        {
            switch (task->state.load())
            {
            case ::BDFlix::DLState::Downloading: return L"\u2193";
            case ::BDFlix::DLState::Complete:    return L"\u2713";
            case ::BDFlix::DLState::Paused:      return L"||";
            case ::BDFlix::DLState::Error:       return L"!";
            case ::BDFlix::DLState::Cancelled:   return L"\u2717";
            default:                              return L"...";
            }
        }

        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush
            StateColor() const;

        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush
            ProgressColor() const;

        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush
            StatusColor() const;

        winrt::hstring StatusLine() const
        {
            using namespace ::BDFlix;
            auto st = task->state.load();
            long long dl = task->totalDl.load();
            long long fs = task->fileSize;
            double spd   = task->speed;

            switch (st)
            {
            case DLState::Downloading:
            {
                std::wstring s = Helpers::FmtSize(dl);
                if (fs > 0)
                {
                    int pct = (int)(dl * 100 / fs);
                    s += L" / " + Helpers::FmtSize(fs) +
                         L"  \u2022  " +
                         std::to_wstring(pct) + L"%";
                }
                if (spd > 0)
                    s += L"  \u2022  " + Helpers::FmtSpeed(spd);
                if (fs > 0 && spd > 0)
                    s += L"  \u2022  " +
                         Helpers::FmtETA(fs - dl, spd);
                return winrt::hstring(s);
            }
            case DLState::Complete:
                return winrt::hstring(
                    L"Complete  \u2022  " +
                    Helpers::FmtSize(fs > 0 ? fs : dl));
            case DLState::Paused:
            {
                std::wstring s = L"Paused  \u2022  " +
                    Helpers::FmtSize(dl);
                if (fs > 0) s += L" / " + Helpers::FmtSize(fs);
                return winrt::hstring(s);
            }
            case DLState::Error:     return L"Failed";
            case DLState::Cancelled: return L"Cancelled";
            default:                 return L"Waiting...";
            }
        }
    };

    // ── Page ────────────────────────────────────────────────────────────────
    struct DownloadsPage : DownloadsPageT<DownloadsPage>
    {
        DownloadsPage();
        ~DownloadsPage();

        void DlList_DoubleTapped(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&);

        void DlList_RightTapped(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const&);

        void CtxPause_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxResume_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxCancel_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxOpenFolder_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void CtxRemove_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void RefreshList();
        void StartTimer();
        void StopTimer();

        winrt::Microsoft::UI::Dispatching::DispatcherQueue
            m_dispatcher{ nullptr };

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
            m_timer{ nullptr };

        winrt::Windows::Foundation::Collections::
            IObservableVector<winrt::Windows::Foundation::IInspectable>
            m_items{ nullptr };

        int m_contextId{ -1 }; // id of task under context menu
    };
}

namespace winrt::BDFlix::factory_implementation
{
    struct DownloadsPage :
        DownloadsPageT<DownloadsPage,
            implementation::DownloadsPage> {};
}
