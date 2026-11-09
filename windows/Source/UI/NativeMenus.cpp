// Android: every pop-up menu and drop-down (JUCE's own, the audio device lists included) as a native Android list
// dialog: finger-sized, the system's look, the current choice marked. android/build.sh patches JUCE's PopupMenu to
// hand menus to levNativePopupMenu. Section headings become labels; a submenu's items are listed as "Menu > item".
#ifdef __ANDROID__
 #define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#endif
#include "NativeMenus.h"

#if JUCE_ANDROID
namespace juce {
extern std::function<bool (const PopupMenu&, std::function<void (const PopupMenu::Item*)>)> levNativePopupMenu;
}

namespace lev {
namespace {

struct Entry { juce::String text; const juce::PopupMenu::Item* item; };   // item null: a heading or disabled

void flatten(const juce::PopupMenu& m, const juce::String& prefix, std::vector<Entry>& out, int& checked) {
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();) {
        const auto& i = it.getItem();
        if (i.isSeparator) continue;
        if (i.isSectionHeader) { out.push_back({ juce::String::fromUTF8("\xe2\x80\x94  ") + i.text.toUpperCase() + juce::String::fromUTF8("  \xe2\x80\x94"), nullptr }); continue; }
        if (i.subMenu != nullptr) { flatten(*i.subMenu, prefix + i.text + juce::String::fromUTF8(" \xe2\x80\xba "), out, checked); continue; }
        if (i.isTicked && checked < 0) checked = (int) out.size();
        out.push_back({ prefix + i.text, i.isEnabled ? &i : nullptr });
    }
}

/// DialogInterface.OnClickListener + OnCancelListener: reports the choice once, on the message thread.
struct Listener : juce::AndroidInterfaceImplementer {
    std::vector<Entry> entries;
    std::function<void(const juce::PopupMenu::Item*)> done;
    bool fired = false;

    jobject invoke(jobject proxy, jobject method, jobjectArray args) override {
        auto* env = juce::getEnv();
        const juce::LocalRef<jclass> methodClass { env->GetObjectClass(method) };
        const juce::LocalRef<jstring> nameJ { (jstring) env->CallObjectMethod(method, env->GetMethodID(methodClass.get(), "getName", "()Ljava/lang/String;")) };
        const auto name = juce::juceString(nameJ.get());
        if (name == "onClick") {
            const juce::LocalRef<jobject> dialog { env->GetObjectArrayElement(args, 0) };
            const juce::LocalRef<jobject> whichObj { env->GetObjectArrayElement(args, 1) };
            const juce::LocalRef<jclass> intClass { env->GetObjectClass(whichObj.get()) };
            const int which = env->CallIntMethod(whichObj.get(), env->GetMethodID(intClass.get(), "intValue", "()I"));
            const juce::LocalRef<jclass> dialogClass { env->GetObjectClass(dialog.get()) };
            env->CallVoidMethod(dialog.get(), env->GetMethodID(dialogClass.get(), "dismiss", "()V"));
            finish(juce::isPositiveAndBelow(which, (int) entries.size()) ? entries[(size_t) which].item : nullptr);
            return nullptr;
        }
        if (name == "onCancel") { finish(nullptr); return nullptr; }
        return AndroidInterfaceImplementer::invoke(proxy, method, args);
    }

    void finish(const juce::PopupMenu::Item* item) {
        if (fired) return;
        fired = true;
        juce::MessageManager::callAsync([d = done, item] { d(item); });
    }
};

bool showNative(const juce::PopupMenu& menu, std::function<void(const juce::PopupMenu::Item*)> done) {
    auto* env = juce::getEnv();
    const auto activity = juce::getMainActivity();
    if (activity == nullptr) return false;

    auto* l = new Listener();
    int checked = -1;
    flatten(menu, {}, l->entries, checked);
    if (l->entries.empty()) { delete l; return false; }
    l->done = std::move(done);
    const auto n = (int) l->entries.size();
    const juce::LocalRef<jclass> stringClass { env->FindClass("java/lang/String") };
    const juce::LocalRef<jobjectArray> items { env->NewObjectArray(n, stringClass.get(), nullptr) };
    for (int i = 0; i < n; ++i) env->SetObjectArrayElement(items.get(), i, juce::javaString(l->entries[(size_t) i].text).get());
    // the interface proxy owns the listener from here
    const auto listener = juce::CreateJavaInterface(l, juce::StringArray { "android/content/DialogInterface$OnClickListener",
                                                                           "android/content/DialogInterface$OnCancelListener" });

    const juce::LocalRef<jclass> builderClass { env->FindClass("android/app/AlertDialog$Builder") };
    constexpr jint themeDeviceDefaultDark = 4;   // AlertDialog.THEME_DEVICE_DEFAULT_DARK
    const juce::LocalRef<jobject> builder { env->NewObject(builderClass.get(), env->GetMethodID(builderClass.get(), "<init>", "(Landroid/content/Context;I)V"),
                                                           activity.get(), themeDeviceDefaultDark) };
    if (checked >= 0) {   // a drop-down: radio buttons with the current choice selected
        const juce::LocalRef<jobject> r { env->CallObjectMethod(builder.get(), env->GetMethodID(builderClass.get(), "setSingleChoiceItems",
            "([Ljava/lang/CharSequence;ILandroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;"), items.get(), checked, listener.get()) };
    } else {
        const juce::LocalRef<jobject> r { env->CallObjectMethod(builder.get(), env->GetMethodID(builderClass.get(), "setItems",
            "([Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;"), items.get(), listener.get()) };
    }
    const juce::LocalRef<jobject> r2 { env->CallObjectMethod(builder.get(), env->GetMethodID(builderClass.get(), "setOnCancelListener",
        "(Landroid/content/DialogInterface$OnCancelListener;)Landroid/app/AlertDialog$Builder;"), listener.get()) };
    const juce::LocalRef<jobject> dialog { env->CallObjectMethod(builder.get(), env->GetMethodID(builderClass.get(), "create", "()Landroid/app/AlertDialog;")) };
    const juce::LocalRef<jclass> dialogClass { env->GetObjectClass(dialog.get()) };
    env->CallVoidMethod(dialog.get(), env->GetMethodID(dialogClass.get(), "show", "()V"));
    return true;
}

/// The OK/Cancel buttons of the text prompt: reads the field on OK.
struct TextListener : juce::AndroidInterfaceImplementer {
    juce::GlobalRef field;
    std::function<void(juce::String)> done;
    jobject invoke(jobject proxy, jobject method, jobjectArray args) override {
        auto* env = juce::getEnv();
        const juce::LocalRef<jclass> methodClass { env->GetObjectClass(method) };
        const juce::LocalRef<jstring> nameJ { (jstring) env->CallObjectMethod(method, env->GetMethodID(methodClass.get(), "getName", "()Ljava/lang/String;")) };
        if (juce::juceString(nameJ.get()) != "onClick") return AndroidInterfaceImplementer::invoke(proxy, method, args);
        const juce::LocalRef<jobject> whichObj { env->GetObjectArrayElement(args, 1) };
        const juce::LocalRef<jclass> intClass { env->GetObjectClass(whichObj.get()) };
        const int which = env->CallIntMethod(whichObj.get(), env->GetMethodID(intClass.get(), "intValue", "()I"));
        if (which == -1 && done) {   // DialogInterface.BUTTON_POSITIVE
            const juce::LocalRef<jclass> fieldClass { env->GetObjectClass(field.get()) };
            const juce::LocalRef<jobject> editable { env->CallObjectMethod(field.get(), env->GetMethodID(fieldClass.get(), "getText", "()Landroid/text/Editable;")) };
            const juce::LocalRef<jclass> objClass { env->FindClass("java/lang/Object") };
            const juce::LocalRef<jstring> text { (jstring) env->CallObjectMethod(editable.get(), env->GetMethodID(objClass.get(), "toString", "()Ljava/lang/String;")) };
            juce::MessageManager::callAsync([d = done, t = juce::juceString(text.get())] { d(t); });
        }
        return nullptr;
    }
};

} // namespace

void installNativeMenus() { juce::levNativePopupMenu = showNative; }

bool nativeAskText(const juce::String& title, const juce::String& initial, const juce::String& okText, std::function<void(juce::String)> done) {
    auto* env = juce::getEnv();
    const auto activity = juce::getMainActivity();
    if (activity == nullptr) return false;
    constexpr jint themeDeviceDefaultDark = 4;
    const juce::LocalRef<jclass> editC { env->FindClass("android/widget/EditText") };
    const juce::LocalRef<jobject> field { env->NewObject(editC.get(), env->GetMethodID(editC.get(), "<init>", "(Landroid/content/Context;)V"), activity.get()) };
    env->CallVoidMethod(field.get(), env->GetMethodID(editC.get(), "setText", "(Ljava/lang/CharSequence;)V"), juce::javaString(initial).get());
    env->CallVoidMethod(field.get(), env->GetMethodID(editC.get(), "setSingleLine", "()V"));
    env->CallVoidMethod(field.get(), env->GetMethodID(editC.get(), "selectAll", "()V"));
    env->CallVoidMethod(field.get(), env->GetMethodID(editC.get(), "setHint", "(Ljava/lang/CharSequence;)V"), juce::javaString("Name").get());

    auto* l = new TextListener();
    l->field = juce::GlobalRef(field);
    l->done = std::move(done);
    const auto listener = juce::CreateJavaInterface(l, "android/content/DialogInterface$OnClickListener");

    const juce::LocalRef<jclass> builderC { env->FindClass("android/app/AlertDialog$Builder") };
    const juce::LocalRef<jobject> builder { env->NewObject(builderC.get(), env->GetMethodID(builderC.get(), "<init>", "(Landroid/content/Context;I)V"),
                                                           activity.get(), themeDeviceDefaultDark) };
    const auto ret = "Landroid/app/AlertDialog$Builder;";
    auto call = [&](const char* m, const juce::String& sig, auto... a) {
        const juce::LocalRef<jobject> r { env->CallObjectMethod(builder.get(), env->GetMethodID(builderC.get(), m, (sig + ret).toRawUTF8()), a...) };
    };
    call("setTitle", "(Ljava/lang/CharSequence;)", juce::javaString(title).get());
    call("setView", "(Landroid/view/View;)", field.get());
    call("setPositiveButton", "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)", juce::javaString(okText).get(), listener.get());
    call("setNegativeButton", "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)", juce::javaString("Cancel").get(), listener.get());
    const juce::LocalRef<jobject> dialog { env->CallObjectMethod(builder.get(), env->GetMethodID(builderC.get(), "create", "()Landroid/app/AlertDialog;")) };
    const juce::LocalRef<jclass> dialogC { env->GetObjectClass(dialog.get()) };
    // open with the keyboard up, ready to type
    const juce::LocalRef<jobject> window { env->CallObjectMethod(dialog.get(), env->GetMethodID(dialogC.get(), "getWindow", "()Landroid/view/Window;")) };
    if (window != nullptr) {
        const juce::LocalRef<jclass> windowC { env->GetObjectClass(window.get()) };
        env->CallVoidMethod(window.get(), env->GetMethodID(windowC.get(), "setSoftInputMode", "(I)V"), (jint) 5 /* SOFT_INPUT_STATE_ALWAYS_VISIBLE */);
    }
    env->CallVoidMethod(dialog.get(), env->GetMethodID(dialogC.get(), "show", "()V"));
    env->CallBooleanMethod(field.get(), env->GetMethodID(editC.get(), "requestFocus", "()Z"));
    return true;
}

} // namespace lev

#else
namespace lev {
void installNativeMenus() {}
bool nativeAskText(const juce::String&, const juce::String&, const juce::String&, std::function<void(juce::String)>) { return false; }
}
#endif
