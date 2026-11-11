#include "Widgets.h"
#include <BinaryData.h>

namespace lev {

// Type: Inter everywhere (bundled), on one fixed scale so sizes stay consistent; the same scale as the Mac app's T().
namespace {
const juce::Typeface::Ptr& interRegular() {
    static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    return t;
}
const juce::Typeface::Ptr& interBold() {   // "bold" is SemiBold: Inter's Bold reads heavy at these small sizes
    static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor(BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    return t;
}
}

float theme::snap(float pt) {
    if (pt > 24) return pt;   // display sizes (logo, tuner note) are their own
    static constexpr float scale[] = { 9, 10.5f, 12, 13, 16, 22 };
    float best = scale[0];
    for (float s : scale)
        if (std::abs(s - pt) < std::abs(best - pt) || (std::abs(s - pt) == std::abs(best - pt) && s > best)) best = s;
    return best;
}

juce::Font theme::font(float pt, bool bold, float tracking) {
    pt = snap(pt);
    juce::Font f(juce::FontOptions(bold ? interBold() : interRegular()).withHeight(pt * UI * 1.18f));
    if (tracking != 0) f.setExtraKerningFactor(tracking / (pt * UI));
    return f;
}

/// "Helga B 5150 BlockLetter - NoBoost" -> "5150 BlockLetter - NoBoost  (Helga B)" (same rule as the Mac app's namTitle).
juce::String namTitle(juce::String n) {
    n = n.replaceCharacter('_', ' ');
    if (const int e = n.indexOfIgnoreCase("ESR"); e > 0) {   // drop the training error suffix ("-ESR0,003", " ESR_0.0115")
        int s = e; while (s > 0 && juce::String("-_ ").containsChar(n[s - 1])) --s;
        int t = e + 3; while (t < n.length() && juce::String("-_ 0123456789.,").containsChar(n[t])) ++t;
        n = n.substring(0, s) + n.substring(t);
    }
    juce::StringArray w; w.addTokens(n, " ", ""); w.removeEmptyStrings();
    if (w.size() > 2 && w[1].length() == 1 && juce::CharacterFunctions::isUpperCase(w[1][0])) {
        const auto who = w[0] + " " + w[1]; w.removeRange(0, 2);
        return w.joinIntoString(" ") + "  (" + who + ")";
    }
    return w.joinIntoString(" ");
}

SourceButton::SourceButton(std::function<juce::String()> b, std::function<juce::String()> n, std::function<juce::String()> d, std::function<void(SourceButton&)> open, int width)
    : badge(std::move(b)), name(std::move(n)), detail(std::move(d)), onOpen(std::move(open)) {
    setPref(*this, (int) ((float) width * theme::W), 40);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void SourceButton::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRoundedRectangle(r, 7);
    g.setColour(juce::Colours::white.withAlpha(0.1f)); g.drawRoundedRectangle(r, 7, 1);
    auto in = getLocalBounds().reduced(8, 3);
    const auto bdg = badge();
    const int bw = juce::GlyphArrangement::getStringWidthInt(theme::font(9, true), bdg) + 10;
    const auto br = in.removeFromLeft(bw).withSizeKeepingCentre(bw, 18);
    g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(br.toFloat(), 4);
    g.setColour(theme::secondary); g.setFont(theme::font(9, true)); g.drawText(bdg, br, juce::Justification::centred);
    in.removeFromLeft(8);
    const auto chev = in.removeFromRight(14);
    g.strokePath(IconButton::pathFor(IconButton::chevronDown, chev.toFloat().withSizeKeepingCentre(10, 10)), juce::PathStrokeType(1.5f));
    g.setColour(theme::text); g.setFont(theme::font(10.5f, true));
    g.drawText(name(), in.removeFromTop(in.getHeight() / 2), juce::Justification::bottomLeft, true);
    g.setColour(theme::secondary); g.setFont(theme::font(9));
    g.drawText(detail(), in, juce::Justification::topLeft, true);
}

void SourceButton::mouseUp(const juce::MouseEvent& e) { if (getLocalBounds().contains(e.getPosition()) && onOpen) onOpen(*this); }

// MARK: - look and feel

LookAndFeel::LookAndFeel() {
    setDefaultSansSerifTypeface(interRegular());   // anything drawn without theme::font
    setColourScheme(juce::LookAndFeel_V4::getDarkColourScheme());
    setColour(juce::ResizableWindow::backgroundColourId, theme::bg);
    setColour(juce::TextButton::buttonColourId, theme::panel2);
    setColour(juce::TextButton::textColourOffId, theme::text);
    setColour(juce::ComboBox::backgroundColourId, theme::panel2);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1c1c1c));
    setColour(juce::TextEditor::backgroundColourId, theme::panel2);
    setColour(juce::TextEditor::outlineColourId, theme::line);
    setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0x55ffffff));
    setColour(juce::Label::textColourId, theme::text);
    setAccent(juce::Colours::red);
}

void LookAndFeel::setAccent(juce::Colour c) {
    accent = c;
    setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    setColour(juce::TextButton::buttonOnColourId, c);
    setColour(juce::ComboBox::textColourId, c);
    setColour(juce::ComboBox::arrowColourId, c);
    setColour(juce::ToggleButton::tickColourId, c);
    setColour(juce::Slider::thumbColourId, c);
    setColour(juce::Slider::trackColourId, c);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha(0.35f));
    setColour(juce::TextEditor::focusedOutlineColourId, c);
    setColour(juce::ProgressBar::foregroundColourId, c);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    auto c = b.getToggleState() ? accent : theme::panel2;
    if (over) c = c.brighter(0.08f);
    if (down) c = c.darker(0.1f);
    g.setColour(c.withMultipliedAlpha(b.isEnabled() ? 1.f : 0.5f));
    g.fillRoundedRectangle(r, std::min(7.f, r.getHeight() / 2));
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    g.setColour(theme::panel2.withMultipliedAlpha(box.isEnabled() ? 1.f : 0.5f));
    g.fillRoundedRectangle(r, 7);
    juce::Path p;
    const float cx = (float) w - 14, cy = (float) h / 2;
    p.startNewSubPath(cx - 3.5f, cy - 2); p.lineTo(cx, cy + 2); p.lineTo(cx + 3.5f, cy - 2);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withMultipliedAlpha(box.isEnabled() ? 1.f : 0.4f));
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// MARK: - layout helpers

void setPref(juce::Component& c, int w, int h) { c.getProperties().set("pw", w); c.getProperties().set("ph", h); }
juce::Point<int> pref(const juce::Component& c) {
    return { (int) c.getProperties().getWithDefault("pw", c.getWidth()), (int) c.getProperties().getWithDefault("ph", c.getHeight()) };
}

void Stack::relayoutPref() {
    int along = 0, across = 0, n = 0;
    for (auto* c : getChildren()) {
        if (!c->isVisible()) continue;
        const auto p = pref(*c);
        along += vert ? p.y : p.x; across = std::max(across, vert ? p.x : p.y); n++;
    }
    along += gap * std::max(n - 1, 0);
    setPref(*this, vert ? across : along, vert ? along : across);
}

void Stack::resized() {
    relayoutPref();
    const auto me = pref(*this);
    int pos = vert ? (getHeight() - me.y) / 2 : (getWidth() - me.x) / 2;
    pos = std::max(pos, 0);
    for (auto* c : getChildren()) {
        if (!c->isVisible()) continue;
        const auto p = pref(*c);
        if (vert) {
            const int x = just.testFlags(juce::Justification::left) ? 0 : (getWidth() - p.x) / 2;
            c->setBounds(x, pos, p.x, p.y); pos += p.y + gap;
        } else {
            c->setBounds(pos, (getHeight() - p.y) / 2, p.x, p.y); pos += p.x + gap;
        }
    }
}

// MARK: - MIDI learn overlay

void LearnOverlay::sync() {
    const bool on = model.midi->learning;
    setInterceptsMouseClicks(on, false);
    repaint();
}

void LearnOverlay::paint(juce::Graphics& g) {
    auto& midi = *model.midi;
    if (!midi.learning) return;
    const bool isArmed = midi.armed && *midi.armed == armed;
    const auto* mapped = midi.mapFor(armed.key);
    const auto acc = model.accent();
    auto b = getLocalBounds().toFloat().reduced(1);
    g.setColour(isArmed ? acc.withAlpha(0.35f) : mapped ? acc.withAlpha(0.15f) : juce::Colours::white.withAlpha(0.04f));
    g.fillRoundedRectangle(b, r);
    // a light tint on every assignable control; a solid outline only on the one you're pointing at or arming
    // (dashed outlines on every control looked like a rendering glitch)
    if (isArmed) { g.setColour(acc); g.drawRoundedRectangle(b, r, 2); }
    else if (isMouseOver()) { g.setColour(acc); g.drawRoundedRectangle(b, r, 1.5f); }
    const auto label = isArmed ? juce::String("move a\ncontrol") : mapped ? "CC " + juce::String(mapped->cc) : juce::String();
    if (label.isNotEmpty()) {
        g.setFont(theme::font(9, true));
        auto tr = b.withSizeKeepingCentre(std::min(b.getWidth(), 64.f), isArmed ? 30.f : 17.f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(tr, 3);
        g.setColour(juce::Colours::white); g.drawFittedText(label, tr.toNearestInt(), juce::Justification::centred, 2);
    }
}

void LearnOverlay::mouseUp(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        if (model.midi->mapFor(armed.key)) {
            juce::PopupMenu menu; menu.addItem("Forget MIDI", [this] { model.midi->forget(armed.key); });
            menu.showMenuAsync({});
        }
        return;
    }
    model.midi->arm(armed);
}

// MARK: - knob

Knob::Knob(Model& m, juce::String l, juce::String k, double low, double high, double d, Fmt f, juce::String scope)
    : model(m), label(std::move(l)), key(std::move(k)), scope(scope), lo(low), hi(high), def(d), fmt(std::move(f)),
      overlay(m, MidiArmed { key, scope + juce::String::fromUTF8(" \xc2\xb7 ") + label, low, high, false }, 10) {
    if (!fmt) fmt = [](double v) { return juce::String(v * 10, 1); };
    setViewportIgnoreDragFlag(true);   // a drag on a dial turns the dial; it never scrolls the page (touch screens)
    addAndMakeVisible(overlay);
    overlay.sync();
    setPref(*this, (int) (60 * theme::W), 120);
    setTooltip(juce::String::fromUTF8("Drag or scroll \xc2\xb7 Shift = fine \xc2\xb7 double-click or \xe2\x86\xba resets"));
}

juce::Rectangle<float> Knob::dial() const { return juce::Rectangle<float>((float) getWidth() / 2 - 33, 0, 66, 66); }

void Knob::resized() { overlay.setBounds(dial().toNearestInt()); }

double Knob::toPos(double v) const {
    v = juce::jlimit(lo, hi, v);
    if (std::isnan(mid)) return (v - lo) / (hi - lo);
    return v < mid ? 0.5 * (v - lo) / (mid - lo) : 0.5 + 0.5 * (v - mid) / (hi - mid);
}

double Knob::fromPos(double t) const {
    t = juce::jlimit(0.0, 1.0, t);
    if (std::isnan(mid)) return lo + t * (hi - lo);
    return t < 0.5 ? lo + (mid - lo) * t / 0.5 : mid + (hi - mid) * (t - 0.5) / 0.5;
}

void Knob::paint(juce::Graphics& g) {
    const auto acc = model.accent();
    const double t = toPos(value());
    const auto c = dial().getCentre();
    // face
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillEllipse(c.x - 22, c.y - 21, 44, 44);
    g.setColour(theme::knobFace); g.fillEllipse(c.x - 22, c.y - 22, 44, 44);
    // track + value arc (270 degrees, starting bottom-left)
    const float start = juce::MathConstants<float>::pi * 1.25f, span = juce::MathConstants<float>::pi * 1.5f;
    juce::Path track; track.addCentredArc(c.x, c.y, 27.5f, 27.5f, 0, -span / 2 - 0, span / 2, true);
    juce::Path arc; arc.addCentredArc(c.x, c.y, 27.5f, 27.5f, 0, -span / 2, -span / 2 + span * (float) t, true);
    juce::ignoreUnused(start);
    const juce::PathStrokeType st(5, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(theme::knobTrack); g.strokePath(track, st);
    if (t > 0.001) { g.setColour(acc); g.strokePath(arc, st); }
    // pointer
    const float ang = -span / 2 + span * (float) t;
    juce::Path ptr; ptr.addRoundedRectangle(-1.5f, -19.f, 3.f, 14.f, 1.5f);
    g.setColour(juce::Colours::white);
    g.fillPath(ptr, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    // value + label
    g.setColour(theme::text); g.setFont(theme::font(11));
    g.drawText(fmt(value()), juce::Rectangle<float>(0, 68, (float) getWidth(), 18), juce::Justification::centred);
    g.setColour(theme::secondary); g.setFont(theme::font(9, true, 0.6f));
    g.drawText(label.toUpperCase(), juce::Rectangle<float>(0, 86, (float) getWidth(), 16), juce::Justification::centred);
    // reset: bright when the knob is away from its default
    const bool atDef = std::abs(value() - def) < 1e-9;
    g.setColour(atDef ? juce::Colour(0xff4d4d4d) : acc);
    g.strokePath(IconButton::pathFor(IconButton::reset, resetArea().withSizeKeepingCentre(13, 13)), juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Knob::mouseDown(const juce::MouseEvent& e) { dragStart = toPos(value()); onReset = resetArea().expanded(4).contains(e.position); }

void Knob::mouseUp(const juce::MouseEvent& e) {
    if (onReset && !e.mouseWasDraggedSinceMouseDown() && resetArea().expanded(4).contains(e.position)) model.set(key, def);
    onReset = false;
}

void Knob::mouseEnter(const juce::MouseEvent&) {
    enteredAt = juce::Time::getMillisecondCounter();
    model.showHover(scope + label, scope.isEmpty() ? label : scope.substring(0, 1) + scope.substring(1).toLowerCase() + juce::String::fromUTF8(" \xc2\xb7 ") + label, Model::explain(key));
}

void Knob::mouseExit(const juce::MouseEvent&) { model.clearHover(scope + label); }

/// Scroll to turn. While the page is scrolling, dials slide under the pointer: those only take the wheel once the
/// pointer has rested on them briefly, and momentum scrolling never turns them.
void Knob::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (w.isInertial || juce::Time::getMillisecondCounter() - enteredAt < 250) { Component::mouseWheelMove(e, w); return; }
    float d = std::abs(w.deltaY) >= std::abs(w.deltaX) ? w.deltaY : -w.deltaX;   // Shift + wheel scrolls sideways on some systems
    if (w.isReversed) d = -d;
    const double step = e.mods.isShiftDown() ? 0.05 : 0.25;   // a mouse-wheel notch (~0.1) moves ~2.5% of the range
    model.set(key, fromPos(toPos(value()) + (double) d * step));
}

void Knob::mouseDrag(const juce::MouseEvent& e) {
    if (onReset) return;
    const bool fine = e.mods.isShiftDown();
    model.set(key, fromPos(dragStart - e.getDistanceFromDragStartY() / (fine ? 800.0 : 180.0)));
}

void Knob::mouseDoubleClick(const juce::MouseEvent&) { model.set(key, def); }

// MARK: - footswitch

Footswitch::Footswitch(Model& m, juce::String k, juce::String name)
    : model(m), key(std::move(k)), overlay(m, MidiArmed { key, name + juce::String::fromUTF8(" \xc2\xb7 On/Off"), 0, 1, true }, 11) {
    addAndMakeVisible(overlay); overlay.sync();
    setPref(*this, 62, 22);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void Footswitch::paint(juce::Graphics& g) {
    const bool on = model.s().b(key);
    const auto acc = model.accent();
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(theme::line); g.drawRoundedRectangle(r, r.getHeight() / 2, 1);
    const float cy = r.getCentreY();
    if (on) { g.setColour(acc.withAlpha(0.5f)); g.fillEllipse(10 - 6, cy - 6, 12, 12); }
    g.setColour(on ? acc : juce::Colour(0xff333333)); g.fillEllipse(10 - 4, cy - 4, 8, 8);
    g.setColour(theme::text); g.setFont(theme::font(9, true, 1));
    g.drawText(on ? "ON" : "OFF", r.withTrimmedLeft(18).withTrimmedRight(6), juce::Justification::centred);
}

void Footswitch::mouseUp(const juce::MouseEvent& e) {
    if (getLocalBounds().contains(e.getPosition())) model.set(key, !model.s().b(key));
}

// MARK: - fader

Fader::Fader(Model& m, int b) : model(m), band(b) { setPref(*this, 26, 165); setViewportIgnoreDragFlag(true); setTooltip(juce::String::fromUTF8("Click or drag \xc2\xb7 double-click resets to 0 dB")); }

void Fader::mouseEnter(const juce::MouseEvent&) {
    static const char* freqs[] = { "80 Hz", "160 Hz", "320 Hz", "640 Hz", "1.2 kHz", "2.5 kHz", "5 kHz", "10 kHz" };
    const auto k = "eq" + juce::String(band);
    model.showHover(k, juce::String::fromUTF8("Graphic EQ \xc2\xb7 ") + freqs[juce::jlimit(0, 7, band)], Model::explain(k));
}

void Fader::setFrom(const juce::MouseEvent& e) {
    const float thumb = 20, h = (float) getHeight();
    const float t = juce::jlimit(0.f, 1.f, 1 - ((float) e.y - thumb / 2) / (h - thumb));
    const double v = std::round((-12 + 24 * t) * 2) / 2;   // 0.5 dB steps
    auto s = model.s(); s.setEq(band, v); model.setSettings(s);
}

void Fader::paint(juce::Graphics& g) {
    const float h = (float) getHeight(), thumb = 20, cx = (float) getWidth() / 2;
    const double v = model.s().eq()[(size_t) band];
    const float t = (float) ((v + 12) / 24);
    const float y = thumb / 2 + (1 - t) * (h - thumb), mid = h / 2;
    g.setColour(theme::knobTrack); g.fillRoundedRectangle(cx - 2, thumb / 2, 4, h - thumb, 2);
    g.setColour(theme::secondary.withAlpha(0.5f)); g.fillRect(cx - 7, mid - 0.5f, 14.f, 1.f);
    g.setColour(model.accent()); g.fillRect(cx - 2, std::min(y, mid), 4.f, std::abs(y - mid));
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(cx - thumb / 2, y - thumb / 2 + 1, thumb, thumb);
    g.setColour(juce::Colours::white); g.fillEllipse(cx - thumb / 2, y - thumb / 2, thumb, thumb);
}

// MARK: - buttons

ActionButton::ActionButton(Model& m, const juce::String& text, std::function<void()> fn, std::optional<MidiArmed> learn) : juce::TextButton(text) {
    onClick = std::move(fn);
    if (learn) { overlay = std::make_unique<LearnOverlay>(m, *learn, 6); addAndMakeVisible(*overlay); overlay->sync(); }
    const int w = juce::GlyphArrangement::getStringWidthInt(theme::font(10), text) + 24;
    setPref(*this, std::max(w, 40), 26);
}

IconButton::IconButton(Icon i, std::function<void()> fn, float size) : juce::Button({}), icon(i), iconSize(size) {
    onClick = std::move(fn);
    setPref(*this, (int) size + 10, (int) size + 8);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

juce::Path IconButton::pathFor(Icon i, juce::Rectangle<float> r) {
    juce::Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight(), cx = r.getCentreX(), cy = r.getCentreY();
    switch (i) {
    case chevronLeft: p.startNewSubPath(cx + w * 0.18f, y + h * 0.15f); p.lineTo(cx - w * 0.18f, cy); p.lineTo(cx + w * 0.18f, y + h * 0.85f); break;
    case chevronRight: p.startNewSubPath(cx - w * 0.18f, y + h * 0.15f); p.lineTo(cx + w * 0.18f, cy); p.lineTo(cx - w * 0.18f, y + h * 0.85f); break;
    case chevronDown: p.startNewSubPath(x + w * 0.15f, cy - h * 0.18f); p.lineTo(cx, cy + h * 0.18f); p.lineTo(x + w * 0.85f, cy - h * 0.18f); break;
    case cross: p.startNewSubPath(x + w * 0.2f, y + h * 0.2f); p.lineTo(x + w * 0.8f, y + h * 0.8f); p.startNewSubPath(x + w * 0.8f, y + h * 0.2f); p.lineTo(x + w * 0.2f, y + h * 0.8f); break;
    case plus: p.startNewSubPath(cx, y + h * 0.15f); p.lineTo(cx, y + h * 0.85f); p.startNewSubPath(x + w * 0.15f, cy); p.lineTo(x + w * 0.85f, cy); break;
    case minus: p.startNewSubPath(x + w * 0.15f, cy); p.lineTo(x + w * 0.85f, cy); break;
    case reset: case refresh: {
        p.addCentredArc(cx, cy, w * 0.36f, h * 0.36f, 0, i == reset ? -2.4f : 0.6f, i == reset ? 2.0f : 5.2f, true);
        const float a = i == reset ? -2.4f : 5.2f, ax = cx + w * 0.36f * std::sin(a), ay = cy - h * 0.36f * std::cos(a);
        p.startNewSubPath(ax - w * 0.18f, ay - h * 0.02f); p.lineTo(ax, ay); p.lineTo(ax + w * 0.02f, ay - h * 0.2f);
        break;
    }
    case star: case starFilled:
        p.addStar({ cx, cy }, 5, w * 0.2f, w * 0.48f, 0); break;
    case folder: p.addRoundedRectangle(x + w * 0.1f, y + h * 0.3f, w * 0.8f, h * 0.55f, 2); p.startNewSubPath(x + w * 0.1f, y + h * 0.3f); p.lineTo(x + w * 0.1f, y + h * 0.18f); p.lineTo(x + w * 0.42f, y + h * 0.18f); p.lineTo(x + w * 0.5f, y + h * 0.3f); break;
    case trash: p.addRoundedRectangle(x + w * 0.25f, y + h * 0.3f, w * 0.5f, h * 0.6f, 1.5f); p.startNewSubPath(x + w * 0.15f, y + h * 0.22f); p.lineTo(x + w * 0.85f, y + h * 0.22f); p.startNewSubPath(x + w * 0.4f, y + h * 0.12f); p.lineTo(x + w * 0.6f, y + h * 0.12f); break;
    case play: p.addTriangle(x + w * 0.25f, y + h * 0.15f, x + w * 0.25f, y + h * 0.85f, x + w * 0.85f, cy); break;
    case pause: p.addRectangle(x + w * 0.25f, y + h * 0.15f, w * 0.17f, h * 0.7f); p.addRectangle(x + w * 0.58f, y + h * 0.15f, w * 0.17f, h * 0.7f); break;
    case prev: p.addTriangle(x + w * 0.8f, y + h * 0.15f, x + w * 0.8f, y + h * 0.85f, x + w * 0.3f, cy); p.addRectangle(x + w * 0.18f, y + h * 0.15f, w * 0.1f, h * 0.7f); break;
    case next: p.addTriangle(x + w * 0.2f, y + h * 0.15f, x + w * 0.2f, y + h * 0.85f, x + w * 0.7f, cy); p.addRectangle(x + w * 0.72f, y + h * 0.15f, w * 0.1f, h * 0.7f); break;
    case grip: for (float f : { 0.3f, 0.5f, 0.7f }) { p.startNewSubPath(x + w * 0.15f, y + h * f); p.lineTo(x + w * 0.85f, y + h * f); } break;
    case metronome:
        p.startNewSubPath(x + w * 0.3f, y + h * 0.88f); p.lineTo(x + w * 0.42f, y + h * 0.12f); p.lineTo(x + w * 0.58f, y + h * 0.12f);
        p.lineTo(x + w * 0.7f, y + h * 0.88f); p.closeSubPath(); p.startNewSubPath(cx, y + h * 0.7f); p.lineTo(x + w * 0.78f, y + h * 0.25f);
        break;
    }
    return p;
}

void IconButton::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre(iconSize, iconSize);
    auto c = getToggleState() ? findColour(juce::TextButton::buttonOnColourId) : colour;
    if (over) c = c.brighter(0.3f);
    if (down) c = c.darker(0.2f);
    if (!isEnabled()) c = c.withMultipliedAlpha(0.35f);
    g.setColour(c);
    const auto p = pathFor(icon, r);
    if (icon == play || icon == pause || icon == prev || icon == next || icon == starFilled) g.fillPath(p);
    else g.strokePath(p, juce::PathStrokeType(icon == grip ? 1.5f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// MARK: - unit box

namespace {
/// The drag handle: starts a drag of its panel.
struct Grip : juce::Component, juce::SettableTooltipClient {
    explicit Grip(juce::String id) : dragId(std::move(id)) { setPref(*this, 18, 18); setMouseCursor(juce::MouseCursor::DraggingHandCursor); setTooltip("Drag to reorder"); setViewportIgnoreDragFlag(true); }
    void paint(juce::Graphics& g) override {
        g.setColour(theme::secondary);
        g.strokePath(IconButton::pathFor(IconButton::grip, getLocalBounds().toFloat().reduced(2)), juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void mouseDrag(const juce::MouseEvent&) override {
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this); c && !c->isDragAndDropActive()) {
            auto* box = findParentComponentOfClass<UnitBox>();
            c->startDragging(dragId, box ? (juce::Component*) box : this, juce::ScaledImage(), true);
        }
    }
    juce::String dragId;
};
} // namespace

UnitBox::UnitBox(Model& m, juce::String t, std::unique_ptr<juce::Component> c, juce::String key, juce::String drag)
    : title(std::move(t)), onKey(std::move(key)), dragId(std::move(drag)), content(std::move(c)), model(m),
      chevron(IconButton::chevronDown, [this] { model.toggleCollapsed(title); }, 11) {
    addAndMakeVisible(chevron);
    chevron.setTooltip(folded() ? "Expand" : "Collapse");
    if (dragId.isNotEmpty()) { gripHandle = std::make_unique<Grip>(dragId); addAndMakeVisible(*gripHandle); }
    if (onKey.isNotEmpty()) { footswitch = std::make_unique<Footswitch>(m, onKey, title); addAndMakeVisible(*footswitch); }
    if (content) { addAndMakeVisible(*content); content->setVisible(!folded()); }
    chevron.icon = folded() ? IconButton::chevronRight : IconButton::chevronDown;
}

juce::Point<int> UnitBox::idealSize(bool expanded) const {
    int head = 14 + 4 + (gripHandle ? 22 : 0) + juce::GlyphArrangement::getStringWidthInt(theme::font(10, true, 2), title) + 12;
    for (auto& c : leading) head += pref(*c).x + 6;
    for (auto& c : trailing) head += pref(*c).x + 6;
    if (footswitch) head += 12 + pref(*footswitch).x;
    int w = head, h = headerH;
    if (content && (expanded || !folded())) {
        const auto p = pref(*content);
        w = std::max(w, p.x); h += gapAfterHeader + p.y;
    }
    return { w + 2 * pad, h + 2 * pad };
}

void UnitBox::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(theme::panel); g.fillRoundedRectangle(r, 12);
    g.setColour(theme::line); g.drawRoundedRectangle(r, 12, 1);
    int x = pad + 14 + 4 + (gripHandle ? 22 : 0);
    for (auto& c : leading) x += pref(*c).x + 6;
    g.setColour(theme::secondary); g.setFont(theme::font(10, true, 2));
    g.drawText(title, x, pad, 400, headerH, juce::Justification::centredLeft);
}

void UnitBox::paintOverChildren(juce::Graphics& g) {
    if (content && content->isVisible() && dimContent()) {
        // switched off: dim the controls like the Mac app (0.4 opacity)
        g.setColour(theme::panel.withAlpha(0.6f));
        g.fillRect(content->getBounds());
    }
}

void UnitBox::mouseMove(const juce::MouseEvent& e) {
    if (e.y > pad + headerH + 4) { model.clearHover("panel:" + title); return; }
    const auto key = dragId.startsWith("panel:") ? dragId : juce::String("panel:pedals");
    model.showHover("panel:" + title, title.substring(0, 1) + title.substring(1).toLowerCase(), Model::explain(key));
}

void UnitBox::mouseExit(const juce::MouseEvent&) { model.clearHover("panel:" + title); }

void UnitBox::resized() {
    int x = pad;
    chevron.setBounds(x, pad + (headerH - 18) / 2, 14, 18); x += 18;
    if (gripHandle) { gripHandle->setBounds(x, pad + (headerH - 18) / 2, 18, 18); x += 22; }
    for (auto& c : leading) { const auto p = pref(*c); c->setBounds(x, pad + (headerH - p.y) / 2, p.x, p.y); x += p.x + 6; }
    x += juce::GlyphArrangement::getStringWidthInt(theme::font(10, true, 2), title) + 12;
    for (auto& c : trailing) { const auto p = pref(*c); c->setBounds(x, pad + (headerH - p.y) / 2, p.x, p.y); x += p.x + 6; }
    if (footswitch) { const auto p = pref(*footswitch); footswitch->setBounds(getWidth() - pad - p.x, pad + (headerH - p.y) / 2, p.x, p.y); }
    if (content) {
        content->setVisible(!folded());
        // centred across, sitting on the bottom so knob rows in a grid row line up
        const auto p = pref(*content);
        const auto area = juce::Rectangle<int>(pad, pad + headerH + gapAfterHeader, getWidth() - 2 * pad, getHeight() - 2 * pad - headerH - gapAfterHeader);
        content->setBounds(area.getX() + (area.getWidth() - p.x) / 2, alignTop ? area.getY() : area.getBottom() - p.y, p.x, p.y);
    }
}

bool UnitBox::isInterestedInDragSource(const SourceDetails& d) {
    const auto id = d.description.toString();
    return dragId.isNotEmpty() && id.upToFirstOccurrenceOf(":", false, false) == dragId.upToFirstOccurrenceOf(":", false, false);
}

void UnitBox::itemDragEnter(const SourceDetails& d) {
    const auto src = d.description.toString();
    if (src == dragId) return;
    const auto kind = src.upToFirstOccurrenceOf(":", false, false);
    const auto from = src.fromFirstOccurrenceOf(":", false, false), to = dragId.fromFirstOccurrenceOf(":", false, false);
    if (kind == "panel") {
        auto order = model.panelOrder;
        const int fi = order.indexOf(from), ti = order.indexOf(to);
        if (fi < 0 || ti < 0) return;
        order.move(fi, ti);
        model.setPanelOrder(order);
    } else if (kind == "pedal") {
        model.movePedal((PedalKind) from.getIntValue(), (PedalKind) to.getIntValue());
    }
}

// MARK: - help strip

void HelpStrip::paint(juce::Graphics& g) {
    g.fillAll(theme::bar);
    g.setColour(theme::line); g.fillRect(0, 0, getWidth(), 1);
    auto r = getLocalBounds().reduced(14, 6);
    g.setColour(theme::secondary);
    g.drawEllipse(juce::Rectangle<float>((float) r.getX(), (float) r.getCentreY() - 7, 14, 14), 1.2f);
    g.setFont(theme::font(10, true)); g.drawText("i", r.getX(), r.getCentreY() - 7, 14, 14, juce::Justification::centred);
    r.removeFromLeft(24);
    if (model.hoverText.isEmpty()) {
        g.setFont(theme::font(12));
        g.drawText(idle, r, juce::Justification::centredLeft, true);
        return;
    }
    g.setColour(theme::text); g.setFont(theme::font(12, true));
    const int tw = juce::GlyphArrangement::getStringWidthInt(theme::font(12, true), model.hoverTitle) + 12;
    g.drawText(model.hoverTitle, r.removeFromLeft(tw), juce::Justification::centredLeft, true);
    g.setColour(theme::secondary); g.setFont(theme::font(12));
    g.drawFittedText(model.hoverText, r, juce::Justification::centredLeft, 2);
}

// MARK: - grid

void layoutGrid(std::vector<UnitBox*> items, juce::Rectangle<int> area, int& usedHeight, int columns, int spacing, bool fill) {
    const float unit = std::max(((float) area.getWidth() - (float) spacing * (float) (columns - 1)) / (float) columns, 1.f);
    struct Item { UnitBox* box; int col, span; juce::Point<int> size; };
    std::vector<std::vector<Item>> rows(1);
    int col = 0;
    for (auto* b : items) {
        if (b == nullptr) { if (col > 0) { rows.emplace_back(); col = 0; } continue; }
        const auto s = b->idealSize(true);
        const int span = std::min(columns, std::max(1, (int) std::ceil(((float) s.x + (float) spacing) / (unit + (float) spacing) - 0.001f)));
        if (col > 0 && col + span > columns) { rows.emplace_back(); col = 0; }
        rows.back().push_back({ b, col, span, s });
        col += span;
    }
    if (fill)
        for (auto& row : rows) {
            int used = 0; for (auto& it : row) used += it.span;
            const int spare = columns - used;
            if (row.empty() || spare <= 0) continue;
            int c = 0, given = 0;
            for (size_t k = 0; k < row.size(); ++k) {
                const int extra = k + 1 == row.size() ? spare - given : spare * row[k].span / used;
                given += extra; row[k].col = c; row[k].span += extra; c += row[k].span;
            }
        }
    int y = area.getY();
    for (auto& row : rows) {
        if (row.empty()) continue;
        // while any panel in the row is open, the row keeps every panel's expanded height; a fully folded row shrinks
        bool anyOpen = false; for (auto& it : row) anyOpen |= !it.box->folded();
        int rh = 0; for (auto& it : row) rh = std::max(rh, anyOpen ? it.size.y : it.box->idealSize().y);
        for (auto& it : row) {
            const int x = area.getX() + (int) std::round((float) it.col * (unit + (float) spacing));
            const int w = (int) std::round((float) it.span * unit + (float) (it.span - 1) * (float) spacing);
            it.box->setBounds(x, y, w, it.box->folded() ? it.box->idealSize().y : rh);
        }
        y += rh + spacing;
    }
    usedHeight = std::max(0, y - area.getY() - spacing);
}

// MARK: - logo

void drawLogo(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    const float s = std::min(r.getWidth(), r.getHeight()) / 100, ox = r.getCentreX() - 50 * s, oy = r.getCentreY() - 50 * s;
    auto pt = [&](float x, float y) { return juce::Point<float>(ox + x * s, oy + y * s); };
    juce::Path p;
    // head: broad back rising from the water, blunt brow, then the upper jaw raking down into the sea
    p.startNewSubPath(pt(8, 86));
    p.cubicTo(pt(4, 58), pt(8, 25), pt(21, 11)); p.cubicTo(pt(29, 2), pt(55, 2), pt(65, 11)); p.cubicTo(pt(70, 15), pt(70, 21), pt(67, 26)); p.lineTo(pt(46, 84));
    // lower jaw
    p.startNewSubPath(pt(48, 86)); p.lineTo(pt(92, 47));
    // teeth
    auto ticks = [&](float ax, float ay, float bx, float by, float nx, float ny) {
        for (int i = 0; i < 5; ++i) { const float t = (float) i / 4, x = ax + (bx - ax) * t, y = ay + (by - ay) * t; p.startNewSubPath(pt(x, y)); p.lineTo(pt(x + nx * 4, y + ny * 4)); }
    };
    ticks(64, 33, 52, 67, 0.9f, 0.42f);
    ticks(60, 75, 85, 53, -0.66f, -0.75f);
    // the sea
    float x = 3; bool up = true; p.startNewSubPath(pt(3, 89));
    while (x < 96.99f) { const float nx = std::min(x + 9.4f, 97.f), dy = up ? -2.2f : 2.2f; p.cubicTo(pt(x + (nx - x) * 0.35f, 89 + dy), pt(x + (nx - x) * 0.65f, 89 + dy), pt(nx, 89)); x = nx; up = !up; }
    g.setColour(c);
    g.strokePath(p, juce::PathStrokeType(std::max(1.f, 100 * s * 0.045f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    p.clear(); p.addEllipse(ox + (25 - 1.8f) * s, oy + (72 - 1.8f) * s, 3.6f * s, 3.6f * s);
    g.strokePath(p, juce::PathStrokeType(std::max(1.f, 100 * s * 0.045f)));
}

} // namespace lev
