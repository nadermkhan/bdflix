#pragma once
#include "App.xaml.g.h"

namespace winrt::BDFlix::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
    };
}

namespace winrt::BDFlix::factory_implementation
{
    struct App : AppT<App, implementation::App> {};
}
