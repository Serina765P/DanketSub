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
                detail: BilingualSubtitleModel.fileName
                SplitView.fillHeight: true
                SplitView.minimumHeight: 180

                BilingualGridView {
                    objectName: "bilingualGridRoot"
                    anchors.fill: parent
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
                Text {
                    visible: BilingualSubtitleModel.lastError.length > 0
                    Layout.maximumWidth: 500
                    text: BilingualSubtitleModel.lastError
                    color: workspace.colors.danger
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("%1 pairs").arg(BilingualSubtitleModel.pairCount)
                    color: workspace.colors.textSecondary
                    font.pixelSize: 11
                }
            }
        }
    }
}
