#include "pch.h"
#include "DownloadsPage.xaml.h"
#if __has_include("DownloadsPage.g.cpp")
#include "DownloadsPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Dispatching.h>

namespace winrt::BDFlix::implementation
{
    // ── DlItemVM colour helpers ──────────────────────────────────────────────
    static Microsoft::UI::Xaml::Media::SolidColorBrush
        BrushFromKey(const wchar_t* key)
    {
        return Application::Current().Resources()
            .Lookup(box_value(winrt::hstring(key)))
            .as<Microsoft::UI::Xaml::Media::SolidColorBrush>();
    }

    Microsoft::UI::Xaml::Media::SolidColorBrush
        DlItemVM::StateColor() const
    {
        switch (task->state.load())
        {
        case ::BDFlix::DLState::Downloading:
            return BrushFromKey(L"PrimaryBrush");
        case ::BDFlix::DLState::Complete:
            return BrushFromKey(L"GreenBrush");
        case ::BDFlix::DLState::Paused:
            return BrushFromKey(L"OrangeBrush");
        case ::BDFlix::DLState::Error:
        case ::BDFlix::DLState::Cancelled:
            return BrushFromKey(L"RedBrush");
        default:
            return BrushFromKey(L"Text3Brush");
        }
    }

    Microsoft::UI::Xaml::Media::SolidColorBrush
        DlItemVM::ProgressColor() const
    {
        return (task->state.load() == ::BDFlix::DLState::Paused)
            ? BrushFromKey(L"OrangeBrush")
            : BrushFromKey(L"PrimaryBrush");
    }

    Microsoft::UI::Xaml::Media::SolidColorBrush
        DlItemVM::StatusColor() const
    {
        switch (task->state.load())
        {
        case ::BDFlix::DLState::Complete:
            return BrushFromKey(L"GreenBrush");
        case ::BDFlix::DLState::Paused:
            return BrushFromKey(L"OrangeBrush");
        case ::BDFlix::DLState::Error:
        case ::BDFlix::DLState::Cancelled:
            return BrushFromKey(L"RedBrush");
        default:
            return BrushFromKey(L"Text3Brush");
        }
    }

    // ── DownloadsPage ────────────────────────────────────────────────────────
    DownloadsPage::DownloadsPage()
    {
        InitializeComponent();
        m_dispatcher =
            Microsoft::UI::Dispatching::DispatcherQueue
                ::GetForCurrentThread();

        m_items = winrt::single_threaded_observable_vector<
            winrt::Windows::Foundation::IInspectable>();
        DlList().ItemsSource(m_items);

        StartTimer();
        RefreshList();
    }

    DownloadsPage::~DownloadsPage()
    {
        StopTimer();
    }

    void DownloadsPage::StartTimer()
    {
        m_timer = m_dispatcher.CreateTimer();
        m_timer.Interval(
            std::chrono::milliseconds(350));
        m_timer.Tick([this](auto, auto)
        {
            ::BDFlix::g_DlManager.UpdateSpeeds();
            RefreshList();
        });
        m_timer.Start();
    }

    void DownloadsPage::StopTimer()
    {
        if (m_timer) m_timer.Stop();
    }

    void DownloadsPage::RefreshList()
    {
        auto tasks = ::BDFlix::g_DlManager.GetTasks();

        // Rebuild if count changed
        if (m_items.Size() != tasks.size())
        {
            m_items.Clear();
            for (auto& t : tasks)
                m_items.Append(winrt::make<DlItemVM>(t));
        }
        else
        {
            // Update existing VMs in-place by replacing
            // (x:Bind with Mode=OneTime won't auto-refresh,
            //  so we replace items)
            for (uint32_t i = 0;
                 i < (uint32_t)tasks.size(); i++)
            {
                m_items.SetAt(i,
                    winrt::make<DlItemVM>(tasks[i]));
            }
        }

        bool empty = tasks.empty();
        EmptyState().Visibility(empty
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed);
    }

    void DownloadsPage::DlList_DoubleTapped(
        IInspectable const&,
        Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&)
    {
        auto sel = DlList().SelectedItem();
        if (!sel) return;
        auto vm = sel.as<DlItemVM>();
        if (!vm) return;
        if (vm->task->state.load() == ::BDFlix::DLState::Complete)
        {
            ShellExecuteW(nullptr, L"open",
                vm->task->savePath.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    void DownloadsPage::DlList_RightTapped(
        IInspectable const&,
        Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
    {
        auto sel = DlList().SelectedItem();
        if (!sel) return;
        auto vm = sel.as<DlItemVM>();
        if (!vm) return;

        m_contextId = vm->task->id;
        auto st = vm->task->state.load();

        // Enable/disable menu items
        CtxPause().IsEnabled(
            st == ::BDFlix::DLState::Downloading);
        CtxResume().IsEnabled(
            st == ::BDFlix::DLState::Paused);
        CtxCancel().IsEnabled(
            st == ::BDFlix::DLState::Downloading ||
            st == ::BDFlix::DLState::Paused      ||
            st == ::BDFlix::DLState::Queued);
        CtxRemove().IsEnabled(
            st == ::BDFlix::DLState::Complete  ||
            st == ::BDFlix::DLState::Error     ||
            st == ::BDFlix::DLState::Cancelled);

        DlContextMenu().ShowAt(
            DlList(),
            e.GetPosition(DlList()));
    }

    void DownloadsPage::CtxPause_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_contextId >= 0)
            ::BDFlix::g_DlManager.Pause(m_contextId);
    }

    void DownloadsPage::CtxResume_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_contextId >= 0)
            ::BDFlix::g_DlManager.Resume(m_contextId);
    }

    void DownloadsPage::CtxCancel_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_contextId >= 0)
            ::BDFlix::g_DlManager.Cancel(m_contextId);
    }

    void DownloadsPage::CtxOpenFolder_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        std::wstring folder = ::BDFlix::Helpers::DlFolder();
        ShellExecuteW(nullptr, L"open",
            folder.c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);
    }

    void DownloadsPage::CtxRemove_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        if (m_contextId >= 0)
        {
            ::BDFlix::g_DlManager.Remove(m_contextId);
            RefreshList();
        }
    }
}
