import QtQuick
import QtQuick.Controls

Button {
    id: btn

    // "secondary" | "primary" | "destructive" | "toggle"
    property string variant: "secondary"
    property bool toggled: highlighted

    leftPadding: 14
    rightPadding: 14
    topPadding: 9
    bottomPadding: 9

    background: Rectangle {
        radius: 10

        color: {
            if (btn.variant === "primary") {
                if (!btn.enabled) return "#181200"
                if (btn.down)    return "#201800"
                if (btn.hovered) return "#1a1500"
                return "#181200"
            }
            if (btn.variant === "destructive") {
                if (btn.down)    return "#1c0c10"
                if (btn.hovered) return "#160a0e"
                return "#100608"
            }
            if (btn.variant === "toggle" || btn.toggled) {
                if (btn.down)    return "#0d2238"
                if (btn.hovered) return "#0a1c30"
                return btn.highlighted ? "#0c2035" : "transparent"
            }
            // secondary (default)
            if (btn.down)    return "#07111c"
            if (btn.hovered) return "#060e18"
            return "transparent"
        }

        border.width: 1
        border.color: {
            if (btn.variant === "primary") {
                if (!btn.enabled) return "#181000"
                if (btn.down)    return "#A08800"
                if (btn.hovered) return "#C09800"
                return "#806800"
            }
            if (btn.variant === "destructive") {
                if (btn.down)    return "#a03050"
                if (btn.hovered) return "#883040"
                return "#662030"
            }
            if (btn.variant === "toggle" || btn.toggled) {
                if (btn.highlighted) return "#386888"
                if (btn.hovered) return "#1a3c55"
                return "#122030"
            }
            // secondary
            if (btn.down)    return "#254460"
            if (btn.hovered) return "#1e3a52"
            return "#152a3c"
        }
    }

    contentItem: Label {
        text: btn.text
        font: btn.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        color: {
            if (btn.variant === "primary") {
                if (!btn.enabled) return "#605000"
                if (btn.down)    return "#FEF102"
                if (btn.hovered) return "#FEF102"
                return "#FEF102"
            }
            if (btn.variant === "destructive") {
                if (btn.down)    return "#ff5868"
                if (btn.hovered) return "#ee4a5a"
                return "#cc3050"
            }
            if (btn.variant === "toggle" || btn.toggled) {
                if (btn.highlighted) return "#90c8e8"
                if (btn.hovered)     return "#6aa8c8"
                return "#426880"
            }
            // secondary
            if (!btn.enabled) return "#283c50"
            if (btn.down)    return "#a8d0e8"
            if (btn.hovered) return "#88b8d8"
            return "#5a8eac"
        }
    }
}
