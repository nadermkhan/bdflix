#pragma once
#include "AboutPage.xaml.g.h"

namespace winrt::BDFlix::implementation
{
    struct AboutPage : AboutPageT<AboutPage>
    {
        AboutPage() { InitializeComponent(); }

        void FbButton_Click(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::BDFlix::factory_implementation
{
    struct AboutPage :
        AboutPageT<AboutPage, implementation::AboutPage> {};
}
