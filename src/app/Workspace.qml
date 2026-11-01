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

/// Placeholder workspace proving the themed surfaces, the ribbon insets and
/// the classic video / waveform / grid proportions. Media surfaces land with
/// the audio-clock and bilingual-grid tickets.
Item {
    id: workspace
    objectName: "workspaceRoot"
    readonly property var colors: ThemeManager.palette

    Rectangle {
        anchors.fill: parent
        z: -1
        objectName: "workspaceBackground"
        color: workspace.colors.windowBg
    }

    function modeName() {
        if (ThemeManager.mode === ThemeManager.Light)
            return qsTr("Light")
        if (ThemeManager.mode === ThemeManager.Dark)
            return qsTr("Dark")
        return qsTr("System")
    }

    component Panel: Rectangle {
        id: panel
        property string heading
        property string detail
        default property alias content: body.data
        color: workspace.colors.surfaceBg
        border.width: 1
        border.color: workspace.colors.borderColor
        radius: 6

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: panel.heading
                    color: workspace.colors.textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    text: panel.detail
                    color: workspace.colors.textSecondary
                    font.pixelSize: 12
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: workspace.colors.borderColor
            }

            Item {
                id: body
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            SplitView {
                SplitView.fillHeight: true
                SplitView.preferredHeight: Math.round(workspace.height * 0.45)
                orientation: Qt.Horizontal

                Panel {
                    heading: qsTr("Video preview")
                    detail: qsTr("No media loaded")
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 320

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Video")
                            color: workspace.colors.textPrimary
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Open a video to preview the frame under the playhead")
                            color: workspace.colors.textSecondary
                            font.pixelSize: 12
                        }
                    }
                }

                Panel {
                    heading: qsTr("Audio waveform")
                    detail: qsTr("Audio master clock")
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 320

                    Canvas {
                        id: waveform
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            var w = width
                            var h = height
                            if (w <= 0 || h <= 0)
                                return

                            var selection = workspace.colors.waveformSelection
                            var accent = workspace.colors.accent
                            var cursor = workspace.colors.cursorColor

                            ctx.reset()
                            ctx.fillStyle = workspace.colors.waveformBg
                            ctx.fillRect(0, 0, w, h)

                            // Selection preview.
                            ctx.fillStyle = Qt.rgba(selection.r, selection.g, selection.b, 0.25)
                            ctx.fillRect(w * 0.30, 0, w * 0.18, h)

                            var bars = Math.max(1, Math.floor(w / 4))
                            for (var i = 0; i < bars; ++i) {
                                var t = i / bars
                                var envelope = 0.18 + 0.72 * Math.abs(Math.sin(i * 0.37) * Math.sin(i * 0.11 + 0.4))
                                if (t > 0.30 && t < 0.48)
                                    envelope *= 1.0
                                var barHeight = Math.max(2, envelope * h * 0.82)
                                ctx.fillStyle = Qt.rgba(accent.r, accent.g, accent.b, 0.78)
                                ctx.fillRect(i * 4, (h - barHeight) / 2, 2, barHeight)
                            }

                            // Playhead.
                            ctx.fillStyle = cursor
                            ctx.fillRect(w * 0.42, 0, 2, h)
                        }
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        Connections {
                            target: ThemeManager
                            function onPaletteChanged() {
                                waveform.requestPaint()
                            }
                        }
                    }
                }
            }

            Panel {
                heading: qsTr("Bilingual subtitle grid")
                detail: qsTr("Preview data")
                SplitView.fillHeight: true
                SplitView.minimumHeight: 180

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 88
                            text: qsTr("Start")
                            color: workspace.colors.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            Layout.preferredWidth: 88
                            text: qsTr("End")
                            color: workspace.colors.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Source")
                            color: workspace.colors.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Target")
                            color: workspace.colors.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: workspace.colors.borderColor
                    }

                    ListView {
                        id: grid
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: ListModel {
                            ListElement {
                                start: "0:00:01.20"
                                end: "0:00:03.80"
                                source: "The sky turned to copper that evening."
                                target: "那天傍晚，天空变成了铜色。"
                            }
                            ListElement {
                                start: "0:00:04.10"
                                end: "0:00:07.45"
                                source: "We kept the radio on all night."
                                target: "我们整夜都开着收音机。"
                            }
                            ListElement {
                                start: "0:00:08.00"
                                end: "0:00:11.30"
                                source: "Nobody remembered who spoke first."
                                target: "没人记得是谁先开口的。"
                            }
                            ListElement {
                                start: "0:00:12.05"
                                end: "0:00:15.60"
                                source: "By morning the storm had passed."
                                target: "到了早晨，暴风雨已经过去了。"
                            }
                            ListElement {
                                start: "0:00:16.20"
                                end: "0:00:19.90"
                                source: "She wrote the last line in pencil."
                                target: "她用铅笔写下了最后一行。"
                            }
                        }

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 32
                            color: index === 1
                                   ? workspace.colors.selectionBg
                                   : (index % 2 === 1 ? workspace.colors.gridZebraBg : "transparent")

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                Text {
                                    Layout.preferredWidth: 88
                                    text: model.start
                                    color: workspace.colors.textSecondary
                                    font.pixelSize: 12
                                    font.family: "Consolas"
                                }
                                Text {
                                    Layout.preferredWidth: 88
                                    text: model.end
                                    color: workspace.colors.textSecondary
                                    font.pixelSize: 12
                                    font.family: "Consolas"
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: model.source
                                    color: workspace.colors.textPrimary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: model.target
                                    color: workspace.colors.textPrimary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            objectName: "workspaceStatusBar"
            Layout.fillWidth: true
            implicitHeight: 24
            radius: 4
            color: workspace.colors.surfaceAltBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12

                Text {
                    text: qsTr("Theme: %1").arg(workspace.modeName())
                    color: workspace.colors.textSecondary
                    font.pixelSize: 11
                }
                Text {
                    text: ThemeManager.paletteName.length > 0
                          ? qsTr("Palette: %1").arg(ThemeManager.paletteName)
                          : qsTr("Palette: built-in")
                    color: workspace.colors.textSecondary
                    font.pixelSize: 11
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("%1 pairs").arg(grid.count)
                    color: workspace.colors.textSecondary
                    font.pixelSize: 11
                }
            }
        }
    }
}
