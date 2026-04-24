#pragma once
#include "MainWindow.xaml.g.h"
#include "SearchPage.xaml.h"
#include "DownloadsPage.xaml.h"
#include "AboutPage.xaml.h"

namespace winrt::BDFlix::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void TabSearch_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void TabDownloads_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void TabAbout_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

        void UpdateStatus(const winrt::hstring& text);

    private:
        void SwitchTab(int tab);
        int m_activeTab{ 0 };

        winrt::Microsoft::UI::Xaml::Controls::Frame  m_frame{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBlock m_status{ nullptr };
    };
}

namespace winrt::BDFlix::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
