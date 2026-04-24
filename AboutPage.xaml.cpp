#include "pch.h"
#include "AboutPage.xaml.h"
#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

#include <winrt/Windows.System.h>

namespace winrt::BDFlix::implementation
{
    void AboutPage::FbButton_Click(
        IInspectable const&,
        Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        // WinUI3 proper way to open URL
        Windows::System::Launcher::LaunchUriAsync(
            Windows::Foundation::Uri(
                L"https://www.facebook.com/nadermahbubkhan"));
    }
}
