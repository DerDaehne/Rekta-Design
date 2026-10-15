/*
    The Rekta login screen, as a place: a kicker with the date, the time as its large light title, the people as a pivot,
    one field and one button; the app bar at the bottom holds the session and the power actions. On arrival the parts
    build in, a few ms apart; a failed sign-in says so in place, under the field.

    Colours come from theme.conf (both themes; "variant" picks one, "reducedMotion=true" makes every motion instant),
    Barlow from fonts/, the words from SDDM's TextConstants, so they follow the system's language. Sizes are logical
    pixels for a 1080-line screen and grow with a larger one that the greeter does not scale. scripts/build-kde.mjs fills
    in durations, curves and icons (the @…@ marks) from the tokens.
*/
import QtQuick
import QtQuick.Shapes
import SddmComponents 2.0

Rectangle {
    id: root
    width: 1920
    height: 1080

    readonly property string variant: config.variant === "light" ? "light" : "dark"
    function c(name) { return config[variant + "_" + name] }
    color: c("bg")

    // A greeter that does not scale (X11 on a 4K screen) gets the same layout, larger.
    readonly property real k: Math.max(1, Math.min(width / 1920, height / 1080))
    function u(n) { return Math.round(n * k) }
    // Reduced motion: every duration 0, so each change lands at once and none is skipped.
    readonly property bool still: config.reducedMotion === "true"
    function d(ms) { return still ? 0 : ms }

    FontLoader { id: barlowLight; source: "fonts/barlow-300.ttf" }
    FontLoader { id: barlowRegular; source: "fonts/barlow-400.ttf" }
    FontLoader { id: barlowSemibold; source: "fonts/barlow-600.ttf" }
    readonly property string family: barlowRegular.name

    TextConstants { id: tc }

    readonly property int side: Math.max(u(48), Math.round(width * 0.08))
    property int userIndex: userModel.lastIndex >= 0 ? userModel.lastIndex : 0
    property int sessionIndex: sessionModel.lastIndex >= 0 ? sessionModel.lastIndex : 0
    property bool busy: false
    property bool failed: false
    property var now: new Date()
    readonly property real hoverOpacity: Number(config.hoverOpacity)

    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.now = new Date() }

    function login() {
        if (busy) return;
        const name = userModel.count > 0 ? users.itemAt(userIndex).login : userField.text;
        failed = false;
        busy = true;
        sddm.login(name, password.text, sessionIndex);
    }

    Connections {
        target: sddm
        function onLoginFailed() {
            root.busy = false;
            root.failed = true;
            password.text = "";
            password.focusInput();
        }
        function onLoginSucceeded() { root.busy = false }
    }

    // Arrival: each part rises 6px and fades in over duration-moderate, stagger apart, in reading order.
    component Arrive: Item {
        id: arrive
        property int order: 0
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
        opacity: 0
        transform: Translate { id: rise; y: root.u(6) }
        SequentialAnimation {
            running: true
            PauseAnimation { duration: root.d(arrive.order * @stagger@) }
            ParallelAnimation {
                NumberAnimation { target: arrive; property: "opacity"; to: 1; duration: root.d(@moderate@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
                NumberAnimation { target: rise; property: "y"; to: 0; duration: root.d(@moderate@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
            }
        }
    }

    // State changes glide over duration-short with the base curve.
    component Glide: ColorAnimation { duration: root.d(@short@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-base@] }

    component Label: Text {
        font.family: root.family
        font.pixelSize: root.u(15)
        color: root.c("text_primary")
        renderType: Text.NativeRendering
    }

    // An icon of the set: 20 units, a 1.6 stroke, square ends.
    component Icon: Shape {
        id: shape
        property string d
        property color tint: root.c("text_primary")
        width: root.u(20)
        height: root.u(20)
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: shape.tint
            strokeWidth: 1.6
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            joinStyle: ShapePath.MiterJoin
            scale: Qt.size(root.k, root.k)
            PathSvg { path: shape.d }
        }
    }

    // The focus ring: 2px in the focus colour, 2px outside what has focus.
    component FocusRing: Rectangle {
        anchors.fill: parent
        anchors.margins: -root.u(4)
        radius: root.u(8)
        color: "transparent"
        border.width: root.u(2)
        border.color: root.c("focus")
    }

    // A button of the app bar: a lowercase word, the hover overlay under the pointer, a focus ring 2px outside.
    component BarButton: Item {
        id: bar
        property alias text: label.text
        property alias icon: glyph.d
        signal clicked
        implicitWidth: row.width + 2 * root.u(12)
        implicitHeight: root.u(48)
        opacity: enabled ? 1 : 0.4
        activeFocusOnTab: enabled
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()
        Rectangle { anchors.fill: parent; radius: root.u(4); color: config.hover; opacity: area.pressed ? 2 * root.hoverOpacity : area.containsMouse ? root.hoverOpacity : 0 }
        FocusRing { visible: bar.activeFocus }
        Row {
            id: row
            anchors.centerIn: parent
            spacing: root.u(6)
            Label { id: label; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: root.u(13); font.weight: Font.DemiBold }
            Icon { id: glyph; visible: d !== ""; anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; onClicked: bar.clicked() }
    }

    // A field: the ground, a 1px edge and radius-s; with focus the edge turns ink and a 2px focus ring stands 2px outside.
    component Field: Item {
        id: field
        property alias text: input.text
        property alias echoMode: input.echoMode
        property string placeholder
        signal accepted
        implicitWidth: root.u(360)
        implicitHeight: root.u(44)
        FocusRing { visible: input.activeFocus }
        Rectangle {
            anchors.fill: parent
            radius: root.u(4)
            color: root.c("bg")
            border.width: 1
            border.color: input.activeFocus ? root.c("text_primary") : root.c("text_contrast_medium")
            Behavior on border.color { Glide {} }
        }
        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: root.u(12)
            anchors.rightMargin: root.u(12)
            verticalAlignment: TextInput.AlignVCenter
            font.family: root.family
            font.pixelSize: root.u(18)
            color: root.c("text_primary")
            selectionColor: root.c("text_contrast_low")
            selectedTextColor: root.c("text_primary")
            passwordCharacter: "•"
            clip: true
            onAccepted: field.accepted()
        }
        Label {
            anchors.left: parent.left
            anchors.leftMargin: root.u(12)
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: root.u(18)
            color: root.c("text_contrast_medium")
            text: field.placeholder
            visible: input.text === ""
        }
        function focusInput() { input.forceActiveFocus() }
    }

    // ------------------------------------------------------------------------------------------------- the place

    Column {
        x: root.side
        y: root.u(48)
        spacing: 0
        Arrive {
            order: 0
            Label { text: Qt.locale().toString(root.now, "dddd d MMMM").toLowerCase(); font.pixelSize: root.u(13); font.weight: Font.DemiBold; color: root.c("text_contrast_high") }
        }
        Arrive {
            order: 1
            Label { text: Qt.formatTime(root.now, "HH:mm"); font.pixelSize: root.u(72); font.weight: Font.Light; font.features: { "tnum": 1 } }
        }
    }

    Column {
        id: place
        x: root.side
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.u(24)
        spacing: root.u(16)

        Arrive {
            order: 2
            Label { text: tc.login.toLowerCase(); font.pixelSize: root.u(13); font.weight: Font.DemiBold; color: root.c("text_contrast_high") }
        }

        // The people, as a pivot: large and light; the current one in ink. A click picks another.
        Arrive {
            order: 3
            visible: userModel.count > 0
            Row {
                spacing: root.u(32)
                Repeater {
                    id: users
                    model: userModel
                    delegate: Label {
                        required property int index
                        required property string name
                        required property string realName
                        readonly property string login: name
                        text: realName !== "" ? realName : name
                        font.pixelSize: root.u(42)
                        font.weight: Font.Light
                        color: index === root.userIndex ? root.c("text_primary") : root.c("text_contrast_medium")
                        Behavior on color { Glide {} }
                        MouseArea { anchors.fill: parent; onClicked: { root.userIndex = parent.index; root.failed = false; password.focusInput() } }
                    }
                }
            }
        }

        Arrive {
            order: 4
            Column {
                spacing: root.u(12)
                Field {
                    id: userField
                    visible: userModel.count === 0
                    placeholder: tc.userName.toLowerCase()
                    onAccepted: password.focusInput()
                }
                Row {
                    spacing: root.u(8)
                    Field {
                        id: password
                        placeholder: tc.password.toLowerCase()
                        echoMode: TextInput.Password
                        onAccepted: root.login()
                        Keys.onEscapePressed: text = ""
                        Keys.onLeftPressed: event => { if (text === "" && root.userIndex > 0) root.userIndex--; else event.accepted = false }
                        Keys.onRightPressed: event => { if (text === "" && root.userIndex < userModel.count - 1) root.userIndex++; else event.accepted = false }

                        // Working on it: after busy-delay a 2px line passes under the field while the system checks.
                        Item {
                            id: lane
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.bottom
                            anchors.topMargin: root.u(6)
                            height: root.u(2)
                            clip: true
                            visible: false
                            Timer { interval: @busy-delay@; running: root.busy; onTriggered: lane.visible = true }
                            Connections { target: root; function onBusyChanged() { if (!root.busy) lane.visible = false } }
                            Rectangle {
                                id: runner
                                width: root.still ? lane.width : lane.width / 3
                                height: lane.height
                                color: root.c("text_primary")
                                NumberAnimation on x { from: -lane.width / 3; to: lane.width; duration: @busy@; loops: Animation.Infinite; running: lane.visible && !root.still }
                            }
                        }
                    }
                    // Sign in: the primary button, filled in ink.
                    Item {
                        id: go
                        width: goLabel.implicitWidth + root.u(32)
                        height: root.u(44)
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.login()
                        Keys.onSpacePressed: root.login()
                        FocusRing { visible: go.activeFocus }
                        Rectangle { anchors.fill: parent; radius: root.u(4); color: root.c("accent") }
                        Rectangle { anchors.fill: parent; radius: root.u(4); color: config.hover; opacity: goArea.pressed ? root.hoverOpacity : 0 }
                        Label { id: goLabel; anchors.centerIn: parent; text: tc.login.toLowerCase(); font.weight: Font.DemiBold; color: root.c("on_accent") }
                        MouseArea { id: goArea; anchors.fill: parent; onClicked: root.login() }
                    }
                }
            }
        }

        // A message in place: what needs the person, here, once, with its status square: fade and 8px from above.
        Item {
            width: root.u(360)
            height: root.u(20)
            Row {
                id: notice
                spacing: root.u(8)
                readonly property bool caps: keyboard.capsLock
                readonly property bool shown: root.failed || caps
                opacity: shown ? 1 : 0
                transform: Translate { y: notice.shown ? 0 : -root.u(8); Behavior on y { NumberAnimation { duration: root.d(@short@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] } } }
                Behavior on opacity { NumberAnimation { duration: root.d(@short@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] } }
                Rectangle { width: root.u(8); height: root.u(8); anchors.verticalCenter: parent.verticalCenter; color: root.failed ? root.c("status_error_full") : root.c("status_warning_full") }
                Label { text: root.failed ? tc.loginFailed : tc.capslockWarning; font.pixelSize: root.u(13); color: root.c("text_contrast_high") }
            }
        }
    }

    // ----------------------------------------------------------------------------------------------- the app bar

    Arrive {
        order: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.u(48)
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.c("text_contrast_low") }

        BarButton {
            id: session
            x: root.side - root.u(12)
            text: sessions.count > 0 ? sessions.itemAt(root.sessionIndex)?.text ?? "" : ""
            icon: "@icon-chevron-up@"
            onClicked: list.open ? list.close() : list.show()
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: root.side - root.u(12)
            BarButton { text: tc.suspend.toLowerCase(); enabled: sddm.canSuspend; onClicked: sddm.suspend() }
            BarButton { text: tc.reboot.toLowerCase(); enabled: sddm.canReboot; onClicked: sddm.reboot() }
            BarButton { text: tc.shutdown.toLowerCase(); enabled: sddm.canPowerOff; onClicked: sddm.powerOff() }
        }
    }

    // The sessions: a floating list above its button, the ground with a 1px edge and radius-s; the current one checked,
    // without a box, as Metro did. It fades in 4px away from its button over duration-float and leaves at once. ↑ ↓ move,
    // Enter picks, Escape closes; focus goes back to the button.
    MouseArea { anchors.fill: parent; visible: list.open; onClicked: list.close() }
    Rectangle {
        id: list
        property bool open: false
        property int cursor: root.sessionIndex
        function show() { cursor = root.sessionIndex; open = true; forceActiveFocus() }
        function close() { open = false; session.forceActiveFocus() }
        function pick(i) { root.sessionIndex = i; open = false; password.focusInput() }
        visible: open
        x: root.side - root.u(12)
        y: root.height - root.u(48) - height - root.u(4) - shift
        property real shift: open ? 0 : -root.u(4)
        width: Math.max(root.u(240), sessionColumn.width + root.u(8))
        height: sessionColumn.height + root.u(8)
        radius: root.u(4)
        color: root.c("bg")
        border.width: 1
        border.color: root.c("text_contrast_medium")
        opacity: open ? 1 : 0
        Behavior on opacity { enabled: list.open; NumberAnimation { duration: root.d(@float@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] } }
        Behavior on shift { enabled: list.open; NumberAnimation { duration: root.d(@float@); easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] } }
        Keys.onEscapePressed: close()
        Keys.onUpPressed: cursor = (cursor + sessions.count - 1) % sessions.count
        Keys.onDownPressed: cursor = (cursor + 1) % sessions.count
        Keys.onReturnPressed: pick(cursor)
        Keys.onEnterPressed: pick(cursor)
        Keys.onSpacePressed: pick(cursor)
        Column {
            id: sessionColumn
            x: root.u(4)
            y: root.u(4)
            Repeater {
                id: sessions
                model: sessionModel
                delegate: Item {
                    required property int index
                    required property string name
                    readonly property string text: name
                    width: Math.max(root.u(232), entry.implicitWidth + root.u(48))
                    height: root.u(32)
                    Rectangle { anchors.fill: parent; color: config.hover; opacity: entryArea.containsMouse || parent.index === list.cursor ? root.hoverOpacity : 0 }
                    Icon { x: root.u(6); anchors.verticalCenter: parent.verticalCenter; d: "@icon-check@"; visible: parent.index === root.sessionIndex }
                    Label { id: entry; x: root.u(34); anchors.verticalCenter: parent.verticalCenter; text: parent.name }
                    MouseArea {
                        id: entryArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: list.cursor = parent.index
                        onClicked: list.pick(parent.index)
                    }
                }
            }
        }
    }

    Component.onCompleted: userModel.count > 0 ? password.focusInput() : userField.focusInput()
}
