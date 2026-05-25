import QtQuick 2.15
import QtQuick.Controls 2.15

Flow {
    property string activeView: "runtime"
    signal viewRequested(string view)

    width: parent ? parent.width : implicitWidth
    height: childrenRect.height
    spacing: 8
    readonly property bool stacked: width < 420

    Repeater {
        model: [
            { label: "Runtime", view: "runtime" },
            { label: "Benchmarks", view: "benchmarks" },
            { label: "Notifications", view: "notifications" },
            { label: "Setup", view: "setup" }
        ]

        Button {
            text: modelData.label
            width: stacked ? parent.width : Math.max(98, implicitWidth)
            checked: activeView === modelData.view
            checkable: true
            onClicked: viewRequested(modelData.view)
        }
    }
}
