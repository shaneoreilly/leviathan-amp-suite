// Android: pop-up menus and drop-downs as native Android list dialogs (see NativeMenus.cpp). Nothing on desktop.
#pragma once
#include <JuceHeader.h>

namespace lev {
/// Call once at startup.
void installNativeMenus();
/// Android: a native text prompt (title, a text field holding `initial`, Cancel and `okText`). Calls done with the
/// text on OK. Returns false where there's none (desktop), so the caller shows its own.
bool nativeAskText(const juce::String& title, const juce::String& initial, const juce::String& okText, std::function<void(juce::String)> done);
}
