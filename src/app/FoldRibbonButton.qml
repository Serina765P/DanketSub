// Copyright (c) 2026, DanketSub contributors
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import QtQuick
import RibbonUI

/// Ribbon button that expands or collapses the given tab bar.
RibbonButton {
    id: control
    property var ribbonBar

    iconSource: control.ribbonBar && control.ribbonBar.folded
                ? RibbonIcons.ChevronDown
                : RibbonIcons.ChevronUp
    text: control.ribbonBar && control.ribbonBar.folded
          ? qsTr("Expand")
          : qsTr("Compact")
    tipText: qsTr("Collapse the ribbon (Ctrl+F1 or double-click a tab)")

    onClicked: {
        if (control.ribbonBar)
            control.ribbonBar.folded = !control.ribbonBar.folded
    }
}
