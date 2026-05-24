import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

RowLayout {
    property string activeView: "runtime"
    signal viewRequested(string view)

    width: parent ? parent.width : implicitWidth
    spacing: 8

    Repeater {
        model: [
            { label: "Runtime", view: "runtime" },
            { label: "Benchmarks", view: "benchmarks" },
            { label: "Notifications", view: "notifications" },
            { label: "Setup", view: "setup" }
        ]

        Button {
            text: modelData.label
            checked: activeView === modelData.view
            checkable: true
            onClicked: viewRequested(modelData.view)
        }
    }

    Item { Layout.fillWidth: true }
}
