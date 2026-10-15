/*
    The Rekta login screen, as a place: a kicker with the date, the time as its large light title, the people as a pivot,
    one field and one button; the app bar at the bottom holds the session and the power actions. On arrival the parts
    build in, a few ms apart; a failed sign-in says so in place, under the field.

    Colours come from theme.conf (both themes; "variant" picks one), Barlow from fonts/. scripts/build-kde.mjs fills in
    durations, curves and icons (the @…@ marks) from the tokens.
*/
import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    width: 1920
    height: 1080

    readonly property string variant: config.variant === "light" ? "light" : "dark"
    function c(name) { return config[variant + "_" + name] }
    color: c("bg")

    FontLoader { id: barlowLight; source: "fonts/barlow-300.ttf" }
    FontLoader { id: barlowRegular; source: "fonts/barlow-400.ttf" }
    FontLoader { id: barlowSemibold; source: "fonts/barlow-600.ttf" }
    readonly property string family: barlowRegular.name

    readonly property int side: Math.max(48, Math.round(width * 0.08))
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
        transform: Translate { id: rise; y: 6 }
        SequentialAnimation {
            running: true
            PauseAnimation { duration: arrive.order * @stagger@ }
            ParallelAnimation {
                NumberAnimation { target: arrive; property: "opacity"; to: 1; duration: @moderate@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
                NumberAnimation { target: rise; property: "y"; to: 0; duration: @moderate@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
            }
        }
    }

    component Label: Text {
        font.family: root.family
        font.pixelSize: 15
        color: root.c("text_primary")
        renderType: Text.NativeRendering
    }

    // An icon of the set: 20 units, a 1.6 stroke, square ends.
    component Icon: Shape {
        id: shape
        property string d
        property color tint: root.c("text_primary")
        width: 20
        height: 20
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: shape.tint
            strokeWidth: 1.6
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            joinStyle: ShapePath.MiterJoin
            PathSvg { path: shape.d }
        }
    }

    // A button of the app bar: a lowercase word, the hover overlay under the pointer, a focus ring 2px outside.
    component BarButton: Item {
        id: bar
        property alias text: label.text
        property alias icon: glyph.d
        signal clicked
        implicitWidth: row.width + 2 * 12
        implicitHeight: 48
        opacity: enabled ? 1 : 0.4
        activeFocusOnTab: enabled
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()
        Rectangle { anchors.fill: parent; radius: 4; color: config.hover; opacity: area.pressed ? 2 * root.hoverOpacity : area.containsMouse ? root.hoverOpacity : 0 }
        Rectangle { anchors.fill: parent; anchors.margins: -4; radius: 8; color: "transparent"; border.width: 2; border.color: root.c("focus"); visible: bar.activeFocus }
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Label { id: label; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 13; font.weight: Font.DemiBold }
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
        implicitWidth: 360
        implicitHeight: 44
        Rectangle { anchors.fill: parent; anchors.margins: -4; radius: 8; color: "transparent"; border.width: 2; border.color: root.c("focus"); visible: input.activeFocus }
        Rectangle {
            anchors.fill: parent
            radius: 4
            color: root.c("bg")
            border.width: 1
            border.color: input.activeFocus ? root.c("text_primary") : root.c("text_contrast_medium")
            Behavior on border.color { ColorAnimation { duration: @short@ } }
        }
        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            verticalAlignment: TextInput.AlignVCenter
            font.family: root.family
            font.pixelSize: 18
            color: root.c("text_primary")
            selectionColor: root.c("text_contrast_low")
            selectedTextColor: root.c("text_primary")
            passwordCharacter: "•"
            clip: true
            onAccepted: field.accepted()
        }
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 18
            color: root.c("text_contrast_medium")
            text: field.placeholder
            visible: input.text === ""
        }
        function focusInput() { input.forceActiveFocus() }
    }

    // ------------------------------------------------------------------------------------------------- the place

    Column {
        x: root.side
        y: 48
        spacing: 0
        Arrive {
            order: 0
            Label { text: Qt.locale().toString(root.now, "dddd d MMMM").toLowerCase(); font.pixelSize: 13; font.weight: Font.DemiBold; color: root.c("text_contrast_high") }
        }
        Arrive {
            order: 1
            Label { text: Qt.formatTime(root.now, "HH:mm"); font.pixelSize: 72; font.weight: Font.Light; font.features: { "tnum": 1 } }
        }
    }

    Column {
        id: place
        x: root.side
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 24
        spacing: 16

        Arrive {
            order: 2
            Label { text: "sign in"; font.pixelSize: 13; font.weight: Font.DemiBold; color: root.c("text_contrast_high") }
        }

        // The people, as a pivot: large and light; the current one in ink. A click picks another.
        Arrive {
            order: 3
            visible: userModel.count > 0
            Row {
                spacing: 32
                Repeater {
                    id: users
                    model: userModel
                    delegate: Label {
                        required property int index
                        required property string name
                        required property string realName
                        readonly property string login: name
                        text: realName !== "" ? realName : name
                        font.pixelSize: 42
                        font.weight: Font.Light
                        color: index === root.userIndex ? root.c("text_primary") : root.c("text_contrast_medium")
                        Behavior on color { ColorAnimation { duration: @short@ } }
                        MouseArea { anchors.fill: parent; onClicked: { root.userIndex = parent.index; root.failed = false; password.focusInput() } }
                    }
                }
            }
        }

        Arrive {
            order: 4
            Column {
                spacing: 12
                Field {
                    id: userField
                    visible: userModel.count === 0
                    placeholder: "user"
                    onAccepted: password.focusInput()
                }
                Row {
                    spacing: 8
                    Field {
                        id: password
                        placeholder: "password"
                        echoMode: TextInput.Password
                        onAccepted: root.login()
                        Keys.onEscapePressed: text = ""
                        Keys.onLeftPressed: event => { if (text === "" && root.userIndex > 0) root.userIndex--; else event.accepted = false }
                        Keys.onRightPressed: event => { if (text === "" && root.userIndex < userModel.count - 1) root.userIndex++; else event.accepted = false }

                        // Working on it: a 2px line passes under the field while the system checks.
                        Item {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.bottom
                            anchors.topMargin: 6
                            height: 2
                            clip: true
                            visible: root.busy
                            Rectangle {
                                width: parent.width / 3
                                height: 2
                                color: root.c("text_primary")
                                NumberAnimation on x { from: -parent.width / 3; to: parent.width; duration: 1200; loops: Animation.Infinite; running: root.busy }
                            }
                        }
                    }
                    // Sign in: the primary button, filled in ink.
                    Item {
                        id: go
                        width: goLabel.implicitWidth + 32
                        height: 44
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.login()
                        Keys.onSpacePressed: root.login()
                        Rectangle { anchors.fill: parent; anchors.margins: -4; radius: 8; color: "transparent"; border.width: 2; border.color: root.c("focus"); visible: go.activeFocus }
                        Rectangle { anchors.fill: parent; radius: 4; color: root.c("accent") }
                        Rectangle { anchors.fill: parent; radius: 4; color: config.hover; opacity: goArea.pressed ? root.hoverOpacity : 0 }
                        Label { id: goLabel; anchors.centerIn: parent; text: "sign in"; font.weight: Font.DemiBold; color: root.c("on_accent") }
                        MouseArea { id: goArea; anchors.fill: parent; onClicked: root.login() }
                    }
                }
            }
        }

        // A message in place: what needs the person, here, once. The status square and its word come first.
        Item {
            width: 360
            height: 20
            Row {
                id: notice
                spacing: 8
                readonly property bool caps: keyboard.capsLock
                visible: root.failed || caps
                opacity: visible ? 1 : 0
                transform: Translate { y: notice.visible ? 0 : -8; Behavior on y { NumberAnimation { duration: @short@ } } }
                Behavior on opacity { NumberAnimation { duration: @short@ } }
                Rectangle { width: 8; height: 8; anchors.verticalCenter: parent.verticalCenter; color: root.failed ? root.c("status_error_full") : root.c("status_warning_full") }
                Label { text: root.failed ? "failed" : "waiting"; font.pixelSize: 13; font.weight: Font.DemiBold }
                Label {
                    text: root.failed ? "that password did not open this account; try it again" : "caps lock is on"
                    font.pixelSize: 13
                    color: root.c("text_contrast_high")
                }
            }
        }
    }

    // ----------------------------------------------------------------------------------------------- the app bar

    Arrive {
        order: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 48
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.c("text_contrast_low") }

        BarButton {
            id: session
            x: root.side - 12
            text: sessions.count > 0 ? sessions.itemAt(root.sessionIndex)?.text ?? "" : ""
            icon: "@icon-chevron-up@"
            onClicked: list.visible = !list.visible
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: root.side - 12
            BarButton { text: "suspend"; enabled: sddm.canSuspend; onClicked: sddm.suspend() }
            BarButton { text: "restart"; enabled: sddm.canReboot; onClicked: sddm.reboot() }
            BarButton { text: "shut down"; enabled: sddm.canPowerOff; onClicked: sddm.powerOff() }
        }
    }

    // The sessions: a floating list above its button, the ground with a 1px edge and radius-s; the current one checked,
    // without a box, as Metro did.
    MouseArea { anchors.fill: parent; visible: list.visible; onClicked: list.visible = false }
    Rectangle {
        id: list
        visible: false
        x: root.side - 12
        y: root.height - 48 - height - 4
        width: Math.max(240, sessionColumn.width + 8)
        height: sessionColumn.height + 8
        radius: 4
        color: root.c("bg")
        border.width: 1
        border.color: root.c("text_contrast_medium")
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: @float@ } }
        Keys.onEscapePressed: visible = false
        Column {
            id: sessionColumn
            x: 4
            y: 4
            Repeater {
                id: sessions
                model: sessionModel
                delegate: Item {
                    required property int index
                    required property string name
                    readonly property string text: name
                    width: Math.max(232, entry.implicitWidth + 48)
                    height: 32
                    Rectangle { anchors.fill: parent; color: config.hover; opacity: entryArea.containsMouse ? root.hoverOpacity : 0 }
                    Icon { x: 6; anchors.verticalCenter: parent.verticalCenter; d: "@icon-check@"; visible: parent.index === root.sessionIndex }
                    Label { id: entry; x: 34; anchors.verticalCenter: parent.verticalCenter; text: parent.name }
                    MouseArea {
                        id: entryArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { root.sessionIndex = parent.index; list.visible = false; password.focusInput() }
                    }
                }
            }
        }
    }

    Component.onCompleted: userModel.count > 0 ? password.focusInput() : userField.focusInput()
}
