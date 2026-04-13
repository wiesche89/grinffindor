import QtQuick
import QtQuick.Controls

Item {
    id: root

    property alias text: label.text
    property string copiedValue: ""
    property bool copyEnabled: true
    property bool showFeedback: true
    property alias color: label.color
    property alias font: label.font
    property alias wrapMode: label.wrapMode
    property alias horizontalAlignment: label.horizontalAlignment
    property alias maximumLineCount: label.maximumLineCount
    property alias elide: label.elide

    implicitWidth: label.implicitWidth
    implicitHeight: Math.max(label.implicitHeight, feedbackLabel.visible ? feedbackLabel.implicitHeight : 0)

    function performCopy() {
        if (!copyEnabled)
            return

        var value = copiedValue && copiedValue.length > 0 ? copiedValue : text
        if (!value || value.length === 0)
            return

        if (PlatformBridge.copyToClipboard(value) && showFeedback)
            feedbackTimer.restart()
    }

    Label {
        id: label
        anchors.fill: parent
        opacity: feedbackTimer.running ? 0.72 : 1.0
    }

    Label {
        id: feedbackLabel
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: showFeedback && feedbackTimer.running
        text: qsTr("Copied")
        color: "#8ab8d0"
        font.pixelSize: Math.max(10, label.font.pixelSize - 1)
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.copyEnabled
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        enabled: root.copyEnabled
        onTapped: root.performCopy()
    }

    Timer {
        id: feedbackTimer
        interval: 900
        repeat: false
    }
}
