#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

// cppwinrt emits the App factory (`winrt_make_BDFlix_App`) in App.g.cpp
// because App is declared in Project.idl. Pull it into this translation
// unit so module.g.cpp can resolve the factory at link time.
#if __has_include("App.g.cpp")
#include "App.g.cpp"
#endif

namespace winrt::BDFlix::implementation
{
    App::App()
    {
        InitializeComponent();

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([this](IInspectable const&,
            Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&)
    {
        m_window = make<MainWindow>();
        m_window.Activate();
    }
}
