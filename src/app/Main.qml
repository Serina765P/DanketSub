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
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import RibbonUI
import DanketSub

RibbonWindow {
    id: window
    width: 1360
    height: 860
    minimumWidth: 1000
    minimumHeight: 640
    title: qsTr("DanketSub")

    // The theme manager owns light/dark/system; hide RibbonUI's independent
    // quick toggle so the two never disagree.
    titleBar.showDarkmodeBtn: false
    titleBar.showStyleSwitch: false

    Component.onCompleted: {
        syncRibbonTheme()
        if (typeof startupPalette !== "undefined" && startupPalette)
            ThemeManager.loadPalette(startupPalette)
    }

    function syncRibbonTheme() {
        if (ThemeManager.mode === ThemeManager.Dark)
            RibbonTheme.themeMode = RibbonThemeType.Dark
        else if (ThemeManager.mode === ThemeManager.Light)
            RibbonTheme.themeMode = RibbonThemeType.Light
        else
            RibbonTheme.themeMode = RibbonThemeType.System
    }

    Connections {
        target: ThemeManager
        function onModeChanged() {
            window.syncRibbonTheme()
        }
    }

    Shortcut {
        sequences: ["Ctrl+F1"]
        onActivated: ribbon.folded = !ribbon.folded
    }

    RibbonTabBar {
        id: ribbon
        objectName: "ribbonTabBar"
        showSettingsBtn: true
        onSettingsBtnClicked: ribbon.setPage(2)

        RibbonTabPage {
            objectName: "homePage"
            title: qsTr("Home")

            RibbonTabGroup {
                text: qsTr("Workspace")
                width: workspaceRow.width + 24

                RowLayout {
                    id: workspaceRow
                    spacing: 4

                    FoldRibbonButton {
                        objectName: "compactRibbonButton"
                        ribbonBar: ribbon
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.Play
                        text: qsTr("Play")
                        enabled: false
                        tipText: qsTr("Playback arrives with the audio clock ticket")
                    }
                }
            }

            RibbonTabGroup {
                text: qsTr("Subtitle")
                width: subtitleRow.width + 24

                RowLayout {
                    id: subtitleRow
                    spacing: 4

                    RibbonButton {
                        iconSource: RibbonIcons.Add
                        text: qsTr("New")
                        enabled: false
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.TextEdit
                        text: qsTr("Edit")
                        enabled: false
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.Style
                        text: qsTr("Style")
                        enabled: false
                    }
                }
            }

            RibbonTabGroup {
                text: qsTr("Media")
                width: mediaRow.width + 24

                RowLayout {
                    id: mediaRow
                    spacing: 4

                    RibbonButton {
                        iconSource: RibbonIcons.FolderOpen
                        text: qsTr("Open")
                        enabled: false
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.Save
                        text: qsTr("Save")
                        enabled: false
                    }
                }
            }
        }

        RibbonTabPage {
            title: qsTr("Subtitles")

            RibbonTabGroup {
                text: qsTr("Bilingual")
                width: bilingualRow.width + 24

                RowLayout {
                    id: bilingualRow
                    spacing: 4

                    RibbonButton {
                        iconSource: RibbonIcons.TextBulletList
                        text: qsTr("Bind pair")
                        enabled: false
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.Link
                        text: qsTr("Sync times")
                        enabled: false
                    }
                }
            }
        }

        ThemeSettingsPage {
            objectName: "themeSettingsPage"
            ribbonBar: ribbon
            paletteDialog: palettePicker
        }
    }

    RibbonView {
        anchors.fill: parent
        isMainView: true

        Workspace {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    FileDialog {
        id: palettePicker
        title: qsTr("Open theme palette")
        parentWindow: window
        options: FileDialog.DontUseNativeDialog
        nameFilters: [qsTr("Theme palette (*.json)"), qsTr("All files (*)")]
        onAccepted: ThemeManager.loadPalette(selectedFile)
    }
}
