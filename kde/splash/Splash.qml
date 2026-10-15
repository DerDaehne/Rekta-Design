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
    // Logical pixels for a 1080-line screen; a larger one that is not scaled gets the mark larger.
    readonly property real k: Math.max(1, Math.min(width / 1920, height / 1080))
    readonly property int t: Math.round(6 * k)   // the line's height, and the square's side

    Item {
        id: mark
        anchors.centerIn: parent
        width: Math.round(216 * root.k)
        height: root.t + Math.round(root.t * 88 / 36) + root.t   // the square, the gap (88/36 of the line, as in the icon), the line

        Rectangle { id: track; y: parent.height - height; width: parent.width; height: root.t; color: "@track@" }
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
            width: root.t
            height: root.t
            color: "@done@"
            opacity: root.stage >= 5 ? 1 : 0
            Behavior on opacity {
                enabled: !root.still
                NumberAnimation { duration: @short@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
            }
        }
    }
}
