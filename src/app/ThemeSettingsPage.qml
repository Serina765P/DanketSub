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
import QtQuick.Controls
import QtQuick.Layouts
import RibbonUI
import DanketSub

/// "View" ribbon tab: light / dark / system switching and JSON palette loading.
RibbonTabPage {
    id: page
    title: qsTr("View")
    property var ribbonBar
    property var paletteDialog

    RibbonTabGroup {
        text: qsTr("Appearance")
        width: appearanceRow.width + 24

        RowLayout {
            id: appearanceRow
            spacing: 4

            RibbonButton {
                iconSource: RibbonIcons.WeatherSunny
                text: qsTr("Light")
                tipText: qsTr("Use the light palette")
                onClicked: ThemeManager.mode = ThemeManager.Light
            }
            RibbonButton {
                iconSource: RibbonIcons.WeatherMoon
                text: qsTr("Dark")
                tipText: qsTr("Use the dark palette")
                onClicked: ThemeManager.mode = ThemeManager.Dark
            }
            RibbonButton {
                iconSource: RibbonIcons.Settings
                text: qsTr("System")
                tipText: qsTr("Follow the Windows app mode")
                onClicked: ThemeManager.mode = ThemeManager.System
            }
        }
    }

    RibbonTabGroup {
        text: qsTr("Palette")
        width: paletteRow.width + 24

        RowLayout {
            id: paletteRow
            spacing: 4

            RibbonButton {
                iconSource: RibbonIcons.FolderOpen
                text: qsTr("Load JSON…")
                tipText: qsTr("Load a palette JSON file; missing keys keep their built-in colours")
                onClicked: {
                    if (page.paletteDialog)
                        page.paletteDialog.open()
                }
            }
            RibbonButton {
                iconSource: RibbonIcons.ArrowReset
                text: qsTr("Reset")
                tipText: qsTr("Return to the built-in palette")
                enabled: ThemeManager.palettePath.length > 0
                onClicked: ThemeManager.resetPalette()
            }
            Text {
                Layout.maximumWidth: 240
                text: ThemeManager.lastError.length > 0
                      ? ThemeManager.lastError
                      : (ThemeManager.paletteName.length > 0
                         ? qsTr("Loaded: %1").arg(ThemeManager.paletteName)
                         : qsTr("Built-in palette"))
                color: ThemeManager.lastError.length > 0
                       ? ThemeManager.palette.danger
                       : ThemeManager.palette.textSecondary
                elide: Text.ElideRight
                font.pixelSize: 12
            }
        }
    }

    RibbonTabGroup {
        text: qsTr("Ribbon")
        width: ribbonRow.width + 24

        RowLayout {
            id: ribbonRow
            spacing: 4

            FoldRibbonButton {
                ribbonBar: page.ribbonBar
            }
        }
    }
}
