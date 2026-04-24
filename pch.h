#pragma once

// Windows
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <dwmapi.h>
#include <shellapi.h>

// WinRT / WinUI3
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <microsoft.ui.xaml.window.h>
#include <Microsoft.UI.Interop.h>

// The cppwinrt-generated `.g.h` headers don't implicitly pull WinUI names into
// the `winrt` namespace the way the XAML compiler's `.xaml.g.h` used to, so
// sources like `Application::Current()` and `Grid::SetColumn(...)` fail to
// resolve. Inject the common using-directives at the pch level so every .cpp
// can reference these types unqualified exactly as before.
namespace winrt
{
    using namespace Microsoft::UI;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Controls::Primitives;
    using namespace Microsoft::UI::Xaml::Data;
    using namespace Microsoft::UI::Xaml::Input;
    using namespace Microsoft::UI::Xaml::Media;
    using namespace Microsoft::UI::Xaml::Navigation;
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Collections;
}

// STL
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <functional>
#include <memory>
#include <ctime>
#include <cmath>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
