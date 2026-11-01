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
import DanketSub

/// Side-by-side bilingual subtitle grid.
///
/// One logical row per pair: the left text column is the source language, the
/// right text column the target language. Timing and text edits are routed
/// through the BilingualSubtitleModel singleton, which owns the ASS document
/// and keeps both physical lines of a pair in sync.
Rectangle {
    id: root
    objectName: "bilingualGridRoot"
    color: root.themeColors.surfaceBg

    readonly property var themeColors: ThemeManager.palette

    // Column 0 is the row number, 1 the binding status, 2/3 the shared
    // timing, 4 the derived duration, 5/6 the text pair, 7/8 the styles and
    // 9 the sync id stored in the ASS Effect field.
    readonly property var columnDefs: [
        { "title": qsTr("#"), "width": 42 },
        { "title": qsTr("Binding"), "width": 78 },
        { "title": qsTr("Start"), "width": 92 },
        { "title": qsTr("End"), "width": 92 },
        { "title": qsTr("Duration"), "width": 64 },
        { "title": qsTr("Source"), "width": 280 },
        { "title": qsTr("Target"), "width": 280 },
        { "title": qsTr("Source style"), "width": 90 },
        { "title": qsTr("Target style"), "width": 90 },
        { "title": qsTr("Sync ID"), "width": 120 }
    ]

    function columnWidth(column) {
        return column >= 0 && column < columnDefs.length ? columnDefs[column].width : 80
    }

    function setRowTime(row, column, text) {
        return column === 2
                ? BilingualSubtitleModel.setStartTime(row, text)
                : BilingualSubtitleModel.setEndTime(row, text)
    }

    function setRowText(row, column, text) {
        return column === 5
                ? BilingualSubtitleModel.setSourceText(row, text)
                : BilingualSubtitleModel.setTargetText(row, text)
    }

    component GridToolButton: Button {
        id: control
        readonly property var colors: ThemeManager.palette

        implicitHeight: 24
        leftPadding: 8
        rightPadding: 8
        topPadding: 2
        bottomPadding: 2
        font.pixelSize: 11

        background: Rectangle {
            radius: 4
            color: !control.enabled ? "transparent"
                   : control.down ? control.colors.selectionBg
                   : control.hovered ? control.colors.gridZebraBg
                   : control.colors.surfaceBg
            border.width: 1
            border.color: control.colors.borderColor
            opacity: control.enabled ? 1.0 : 0.5
        }

        contentItem: Text {
            text: control.text
            font: control.font
            color: control.enabled ? control.colors.textPrimary : control.colors.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            objectName: "bilingualToolbar"
            Layout.fillWidth: true
            implicitHeight: 34
            color: root.themeColors.surfaceAltBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.themeColors.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 8
                spacing: 6

                GridToolButton {
                    objectName: "bilingualAddPairButton"
                    text: qsTr("Add pair")
                    onClicked: BilingualSubtitleModel.appendPair()
                }
                GridToolButton {
                    objectName: "bilingualUnbindButton"
                    text: qsTr("Unbind")
                    enabled: BilingualSubtitleModel.pairCount > 0
                    onClicked: BilingualSubtitleModel.unbindRow(BilingualSubtitleModel.currentRow)
                }
                GridToolButton {
                    objectName: "bilingualDeleteSourceButton"
                    text: qsTr("Delete source")
                    enabled: BilingualSubtitleModel.pairCount > 0
                    onClicked: BilingualSubtitleModel.deleteSide(BilingualSubtitleModel.currentRow, true)
                }
                GridToolButton {
                    objectName: "bilingualDeleteTargetButton"
                    text: qsTr("Delete target")
                    enabled: BilingualSubtitleModel.pairCount > 0
                    onClicked: BilingualSubtitleModel.deleteSide(BilingualSubtitleModel.currentRow, false)
                }
                GridToolButton {
                    objectName: "bilingualDeleteRowButton"
                    text: qsTr("Delete row")
                    enabled: BilingualSubtitleModel.pairCount > 0
                    onClicked: BilingualSubtitleModel.deleteRow(BilingualSubtitleModel.currentRow)
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    Layout.maximumWidth: 360
                    text: BilingualSubtitleModel.fileName
                          + (BilingualSubtitleModel.modified ? " *" : "")
                    color: root.themeColors.textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        Item {
            objectName: "bilingualHeader"
            Layout.fillWidth: true
            implicitHeight: 26
            clip: true

            Rectangle {
                anchors.fill: parent
                color: root.themeColors.surfaceBg
            }

            Row {
                id: headerRow
                x: -table.contentX
                height: parent.height

                Repeater {
                    model: root.columnDefs

                    delegate: Item {
                        width: modelData.width
                        height: headerRow.height

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 4
                            verticalAlignment: Text.AlignVCenter
                            text: modelData.title
                            color: root.themeColors.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.themeColors.borderColor
            }
        }

        TableView {
            id: table
            objectName: "bilingualTableView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: BilingualSubtitleModel
            reuseItems: false
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: function(column) { return root.columnWidth(column) }
            rowHeightProvider: function(row) { return 32 }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: cell
                required property int row
                required property int column
                required property var display
                required property bool isLinked

                readonly property bool selected: cell.row === BilingualSubtitleModel.currentRow
                readonly property bool editable: cell.column === 2 || cell.column === 3
                                                 || cell.column === 5 || cell.column === 6
                readonly property bool readOnlyCell: !cell.editable && cell.column !== 1

                Rectangle {
                    anchors.fill: parent
                    color: cell.selected
                           ? root.themeColors.selectionBg
                           : (cell.row % 2 === 1
                              ? root.themeColors.gridZebraBg
                              : root.themeColors.surfaceBg)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: root.themeColors.borderColor
                    opacity: 0.5
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: BilingualSubtitleModel.currentRow = cell.row
                }

                // Binding status pill.
                Rectangle {
                    visible: cell.column === 1
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, Math.min(60, cell.width - 16))
                    height: 20
                    radius: 10
                    color: cell.isLinked ? root.themeColors.accent : root.themeColors.danger

                    Text {
                        anchors.centerIn: parent
                        text: cell.display
                        color: root.themeColors.windowBg
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Read-only cells: row number, duration, styles and sync id.
                Text {
                    visible: cell.readOnlyCell
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: cell.column === 0 ? Text.AlignRight : Text.AlignLeft
                    text: cell.display
                    color: cell.column === 0 || cell.column === 4 || cell.column === 9
                           ? root.themeColors.textSecondary
                           : root.themeColors.textPrimary
                    font.pixelSize: 12
                    font.family: cell.column === 4 || cell.column === 9 ? "Consolas" : ""
                    elide: Text.ElideRight
                }

                // Shared pair timing. Editing either side re-times both lines.
                TextField {
                    id: timeEditor
                    objectName: "bilingualTimeEditor_" + cell.row + "_" + cell.column
                    visible: cell.column === 2 || cell.column === 3
                    anchors.fill: parent
                    anchors.margins: 2
                    text: cell.display
                    color: root.themeColors.textPrimary
                    font.pixelSize: 12
                    font.family: "Consolas"
                    selectByMouse: true
                    leftPadding: 6
                    rightPadding: 6
                    verticalAlignment: TextInput.AlignVCenter

                    background: Rectangle {
                        radius: 3
                        color: timeEditor.activeFocus ? root.themeColors.surfaceBg : "transparent"
                        border.width: timeEditor.activeFocus ? 1 : 0
                        border.color: root.themeColors.accent
                    }

                    onActiveFocusChanged: {
                        if (activeFocus)
                            BilingualSubtitleModel.currentRow = cell.row
                    }
                    onEditingFinished: {
                        if (!root.setRowTime(cell.row, cell.column, timeEditor.text))
                            timeEditor.text = cell.display
                    }
                }

                // Source and target text. Clearing one side of a pair unbinds
                // the counterpart through the model.
                TextField {
                    id: textEditor
                    objectName: "bilingualTextEditor_" + cell.row + "_" + cell.column
                    visible: cell.column === 5 || cell.column === 6
                    anchors.fill: parent
                    anchors.margins: 2
                    text: cell.display
                    color: root.themeColors.textPrimary
                    font.pixelSize: 12
                    selectByMouse: true
                    leftPadding: 6
                    rightPadding: 6
                    verticalAlignment: TextInput.AlignVCenter

                    background: Rectangle {
                        radius: 3
                        color: textEditor.activeFocus ? root.themeColors.surfaceBg : "transparent"
                        border.width: textEditor.activeFocus ? 1 : 0
                        border.color: root.themeColors.accent
                    }

                    onActiveFocusChanged: {
                        if (activeFocus)
                            BilingualSubtitleModel.currentRow = cell.row
                    }
                    onEditingFinished: {
                        if (!root.setRowText(cell.row, cell.column, textEditor.text))
                            textEditor.text = cell.display
                    }
                }
            }
        }
    }
}
