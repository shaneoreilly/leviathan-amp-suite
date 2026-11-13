import SwiftUI

// One dropdown for the whole rig: a box with the choice and ▾, opening a list (the current one in the accent colour).
// Input, Master, Channel, Delay timing, Add pedal and the two Pitch rows all use it, so every dropdown looks and reads
// the same. Windows / Android: LookAndFeel::drawComboBox draws its combo boxes as the same box.

/// The closed box: optional icon, text, ▾.
struct DropdownBox<Icon: View>: View {
    @EnvironmentObject var m: Model
    let text: String
    var width: CGFloat
    var open = false
    var dim = false
    @ViewBuilder let icon: () -> Icon
    var body: some View {
        HStack(spacing: 8) {
            icon()
            Text(text).font(DROPDOWN_FONT).lineLimit(1)
            Spacer(minLength: 4)
            Image(systemName: "chevron.down").font(.system(size: 10 * Z, weight: .semibold)).foregroundStyle(.secondary)
        }
        .foregroundStyle(dim ? .secondary : .primary)
        .padding(.horizontal, 8)
        .frame(width: width, height: 30 * Z)
        .background(RoundedRectangle(cornerRadius: 7).fill(PAL.tint.opacity(0.06)))
        .overlay(RoundedRectangle(cornerRadius: 7).stroke(open ? m.accent : PAL.tint.opacity(0.1)))
        .contentShape(Rectangle())
    }
}

extension DropdownBox where Icon == EmptyView {
    init(text: String, width: CGFloat, open: Bool = false, dim: Bool = false) {
        self.init(text: text, width: width, open: open, dim: dim) { EmptyView() }
    }
}

final class DropdownUI: ObservableObject { @Published var open = false }

/// A row in a dropdown's list: name on the left, an optional detail on the right; the current one in the accent colour.
struct ListRow: View {
    @EnvironmentObject var m: Model
    let title: String, detail: String, on: Bool, enabled: Bool
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            HStack {
                Text(title).fontWeight(on ? .semibold : .regular).lineLimit(1)
                Spacer(minLength: 12)
                if !detail.isEmpty { Text(detail).font(.inter(size: T(9) * UI)).foregroundStyle(.secondary).lineLimit(1) }
            }
            .foregroundStyle(on ? m.accent : .primary)
            .opacity(enabled ? 1 : 0.45)
            .padding(.horizontal, 12).padding(.vertical, 6)
            .background(RoundedRectangle(cornerRadius: 6).fill(on ? m.accent.opacity(0.12) : .clear))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain).disabled(!enabled)
    }
}

/// Pick one of a list. `options`: value, name, and an optional detail shown on the right in the list.
struct Dropdown<T: Hashable>: View {
    @EnvironmentObject var m: Model
    @Binding var selection: T
    let options: [(value: T, name: String, detail: String)]
    var width: CGFloat = 120 * W
    var placeholder = ""
    @StateObject private var ui = DropdownUI()
    var body: some View {
        let current = options.first { $0.value == selection }?.name ?? placeholder
        Button { ui.open.toggle() } label: { DropdownBox(text: current, width: width, open: ui.open) }
            .buttonStyle(.plain)
            .popover(isPresented: $ui.open, arrowEdge: .bottom) {
                DropdownList(rows: options.map { o in (o.name, o.detail, o.value == selection, { selection = o.value; ui.open = false }) })
                    .environmentObject(m)
            }
    }
}

/// A list of actions behind a box (Add pedal).
struct ActionDropdown: View {
    @EnvironmentObject var m: Model
    let title: String
    let items: [(name: String, action: () -> Void)]
    var width: CGFloat = 130 * W
    var disabled = false
    @StateObject private var ui = DropdownUI()
    var body: some View {
        Button { ui.open.toggle() } label: { DropdownBox(text: title, width: width, open: ui.open, dim: disabled) }
            .buttonStyle(.plain).disabled(disabled)
            .popover(isPresented: $ui.open, arrowEdge: .bottom) {
                DropdownList(rows: items.map { i in (i.name, "", false, { ui.open = false; i.action() }) }).environmentObject(m)
            }
    }
}

/// The open list: scrolls when it's long.
struct DropdownList: View {
    let rows: [(title: String, detail: String, on: Bool, action: () -> Void)]
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 1) {
                ForEach(rows.indices, id: \.self) { i in ListRow(title: rows[i].title, detail: rows[i].detail, on: rows[i].on, enabled: true, action: rows[i].action) }
            }.padding(6)
        }
        .font(.inter(size: T(10.5) * UI))
        .frame(minWidth: 260 * Z, maxHeight: min(CGFloat(rows.count) * 31 * Z + 14, 420 * Z))
    }
}
