#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "SearchPage.xaml.h"
#include "DownloadsPage.xaml.h"
#include "AboutPage.xaml.h"

namespace winrt::BDFlix::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Enable dark title bar
        auto hwnd = GetWindowFromWindowId(AppWindow().Id());
        // WinUI3 does this automatically in dark theme,
        // but we can also set it manually:
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd,
            DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        AppWindow().Title(L"BDFlix");
        AppWindow().Resize({ 960, 680 });

        // Cache refs
        m_frame  = ContentFrame();
        m_status = StatusText();

        // Navigate to Search page by default
        SwitchTab(0);
    }

    void MainWindow::TabSearch_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        SwitchTab(0);
    }

    void MainWindow::TabDownloads_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        SwitchTab(1);
    }

    void MainWindow::TabAbout_Click(IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        SwitchTab(2);
    }

    void MainWindow::SwitchTab(int tab)
    {
        m_activeTab = tab;

        // Move indicator
        auto indicator = TabIndicator();
        Grid::SetColumn(indicator, tab);

        // Navigate Frame
        switch (tab)
        {
        case 0:
            m_frame.Navigate(
                xaml_typename<BDFlix::SearchPage>());
            break;
        case 1:
            m_frame.Navigate(
                xaml_typename<BDFlix::DownloadsPage>());
            break;
        case 2:
            m_frame.Navigate(
                xaml_typename<BDFlix::AboutPage>());
            break;
        }
    }

    void MainWindow::UpdateStatus(const winrt::hstring& text)
    {
        m_status.Text(text);
    }
}
