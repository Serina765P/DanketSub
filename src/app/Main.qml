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

    property string pendingFileAction: ""

    function openSubtitles() {
        if (BilingualSubtitleModel.modified) {
            pendingFileAction = "open"
            discardDialog.open()
        } else {
            subtitleOpenDialog.open()
        }
    }

    function newSubtitles() {
        if (BilingualSubtitleModel.modified) {
            pendingFileAction = "new"
            discardDialog.open()
        } else {
            BilingualSubtitleModel.newDocument()
        }
    }

    function saveSubtitles() {
        if (BilingualSubtitleModel.filePath.length > 0)
            BilingualSubtitleModel.save()
        else
            subtitleSaveDialog.open()
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

    Shortcut {
        sequences: ["Ctrl+O"]
        onActivated: window.openSubtitles()
    }

    Shortcut {
        sequences: ["Ctrl+S"]
        onActivated: window.saveSubtitles()
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
                        tipText: qsTr("Start an empty bilingual subtitle document")
                        onClicked: window.newSubtitles()
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
                        tipText: qsTr("Open an ASS or SSA subtitle file (Ctrl+O)")
                        onClicked: window.openSubtitles()
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.Save
                        text: qsTr("Save")
                        tipText: qsTr("Save the current subtitle file (Ctrl+S)")
                        onClicked: window.saveSubtitles()
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
                        objectName: "ribbonAddPairButton"
                        iconSource: RibbonIcons.Add
                        text: qsTr("Add pair")
                        tipText: qsTr("Append a new linked source and target row")
                        onClicked: BilingualSubtitleModel.appendPair()
                    }
                    RibbonButton {
                        iconSource: RibbonIcons.TextBulletList
                        text: qsTr("Bind pair")
                        enabled: false
                        tipText: qsTr("Select one source and one target line, then bind them into a pair")
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

    MessageDialog {
        id: discardDialog
        objectName: "subtitleDiscardDialog"
        title: qsTr("Unsaved changes")
        text: qsTr("Discard unsaved changes?")
        buttons: MessageDialog.Yes | MessageDialog.No
        onAccepted: {
            if (window.pendingFileAction === "new")
                BilingualSubtitleModel.newDocument()
            else if (window.pendingFileAction === "open")
                subtitleOpenDialog.open()
            window.pendingFileAction = ""
        }
        onRejected: window.pendingFileAction = ""
    }

    FileDialog {
        id: subtitleOpenDialog
        objectName: "subtitleOpenDialog"
        title: qsTr("Open subtitles")
        parentWindow: window
        fileMode: FileDialog.OpenFile
        options: FileDialog.DontUseNativeDialog
        nameFilters: [qsTr("Subtitle files (*.ass *.ssa)"), qsTr("All files (*)")]
        onAccepted: BilingualSubtitleModel.loadFile(selectedFile)
    }

    FileDialog {
        id: subtitleSaveDialog
        objectName: "subtitleSaveDialog"
        title: qsTr("Save subtitles")
        parentWindow: window
        fileMode: FileDialog.SaveFile
        defaultSuffix: "ass"
        options: FileDialog.DontUseNativeDialog
        nameFilters: [qsTr("Subtitle files (*.ass *.ssa)"), qsTr("All files (*)")]
        onAccepted: BilingualSubtitleModel.saveFile(selectedFile)
    }
}
