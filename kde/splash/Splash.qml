/*
    The splash of the Rekta global themes: the mark of the app icon. Its line fills as Plasma starts, one step per stage,
    and the square over its end appears when Plasma is ready: done, in the success colour. Nothing else moves.

    scripts/build-kde.mjs fills in the colours and durations (the @…@ marks) from the tokens.
*/
import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    id: root
    color: "@bg@"

    property int stage   // ksplash counts it up as Plasma starts; the last one before it closes is 5

    // Animations off in System Settings: every change lands at once.
    readonly property bool still: Kirigami.Units.longDuration <= 1
    readonly property real progress: Math.min(stage, 5) / 5

    Item {
        id: mark
        anchors.centerIn: parent
        width: 216
        height: 6 + 15 + 6   // the square, the gap above the line (88/36 of its height, as in the icon), the line

        Rectangle { id: track; y: parent.height - height; width: parent.width; height: 6; color: "@track@" }
        Rectangle {
            y: track.y
            width: track.width * root.progress
            height: track.height
            color: "@ink@"
            Behavior on width {
                enabled: !root.still
                NumberAnimation { duration: @moderate@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-base@] }
            }
        }
        Rectangle {
            x: parent.width - width
            width: 6
            height: 6
            color: "@done@"
            opacity: root.stage >= 5 ? 1 : 0
            Behavior on opacity {
                enabled: !root.still
                NumberAnimation { duration: @short@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
            }
        }
    }
}
