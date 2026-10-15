/*
    Leaving, as a Rekta place on the glass ground: who is leaving as the large light title, what happens and when as the
    kicker above it, with a line that runs out with the countdown (as a toast shows its undo window closing), the notes
    as fact lines, and the actions as buttons with a 2px ink edge; the one that the request asked for is filled in ink
    and has focus. The parts build in on arrival. Escape, or a click on the ground, cancels.

    The words are Plasma's own (the Breeze domain), so they come translated. scripts/build-kde.mjs fills in the colours
    and durations (the @…@ marks) from the tokens.
*/
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.coreaddons as KCoreAddons
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.sessions
import "timer.js" as AutoTriggerTimer

Item {
    id: root
    height: screenGeometry.height
    width: screenGeometry.width

    signal logoutRequested()
    signal haltRequested()
    signal haltUpdateRequested()
    signal suspendRequested(int spdMethod)
    signal rebootRequested()
    signal rebootRequested2(int opt)
    signal rebootUpdateRequested()
    signal cancelRequested()
    signal lockScreenRequested()
    signal cancelSoftwareUpdateRequested()

    function sleepRequested() { root.suspendRequested(2) }
    function hibernateRequested() { root.suspendRequested(4) }
    function tr(text) { return i18nd("plasma_lookandfeel_org.kde.lookandfeel", text) }
    function word(text) { return text.replace("&", "").toLowerCase() }   // a UI word: no mnemonic, lowercase

    property real timeout: 30
    property real remainingTime: root.timeout
    readonly property bool showAllOptions: sdtype === ShutdownType.ShutdownTypeDefault
    readonly property bool still: Kirigami.Units.longDuration <= 1
    property var currentAction: {
        switch (sdtype) {
        case ShutdownType.ShutdownTypeReboot:
            return () => softwareUpdatePending ? rebootUpdateRequested() : rebootRequested();
        case ShutdownType.ShutdownTypeHalt:
            return () => softwareUpdatePending ? haltUpdateRequested() : haltRequested();
        default:
            return () => logoutRequested();
        }
    }

    KCoreAddons.KUser { id: kuser }
    SessionsModel { id: otherSessions; includeUnusedSessions: false; includeOwnSession: false }
    QQC2.Action { shortcut: "Escape"; onTriggered: root.cancelRequested() }

    onRemainingTimeChanged: if (remainingTime <= 0) (currentAction)()
    Timer {
        id: countDown
        running: !root.showAllOptions
        repeat: true
        interval: 1000
        onTriggered: root.remainingTime--
        Component.onCompleted: AutoTriggerTimer.addCancelAutoTriggerCallback(() => { countDown.running = false })
    }
    Keys.onPressed: event => { AutoTriggerTimer.cancelAutoTrigger(); event.accepted = false }

    // The ground, as glass: the desktop stays faintly there behind it.
    Rectangle { anchors.fill: parent; color: "@bg@"; opacity: @glass@ }
    MouseArea { anchors.fill: parent; onClicked: root.cancelRequested() }

    // Arrival: each part rises 6px and fades in over duration-moderate, stagger apart.
    component Arrive: Item {
        id: arrive
        property int order: 0
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
        opacity: root.still ? 1 : 0
        transform: Translate { id: rise; y: root.still ? 0 : 6 }
        SequentialAnimation {
            running: !root.still
            PauseAnimation { duration: arrive.order * @stagger@ }
            ParallelAnimation {
                NumberAnimation { target: arrive; property: "opacity"; to: 1; duration: @moderate@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
                NumberAnimation { target: rise; property: "y"; to: 0; duration: @moderate@; easing.type: Easing.Bezier; easing.bezierCurve: [@ease-in@] }
            }
        }
    }

    component Label: Text {
        font.family: Kirigami.Theme.defaultFont.family
        font.pixelSize: 15
        color: "@ink@"
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }

    // An action: a lowercase word in a 2px ink edge; primary fills with ink. Hover lays the overlay on it, focus puts the
    // focus ring 2px outside.
    component Action: Item {
        id: action
        property string text
        property bool primary: false
        signal clicked
        implicitWidth: label.implicitWidth + 2 * 16
        implicitHeight: 44
        activeFocusOnTab: visible
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        Keys.onSpacePressed: clicked()
        Rectangle { anchors.fill: parent; anchors.margins: -4; radius: 8; color: "transparent"; border.width: 2; border.color: "@focus@"; visible: action.activeFocus }
        Rectangle { anchors.fill: parent; radius: 4; color: action.primary ? "@accent@" : "transparent"; border.width: 2; border.color: action.primary ? "@accent@" : "@ink@" }
        Rectangle { anchors.fill: parent; radius: 4; color: "@hover@"; opacity: area.pressed ? 2 * @hoverOpacity@ : area.containsMouse ? @hoverOpacity@ : 0 }
        Label { id: label; anchors.centerIn: parent; text: action.text; font.weight: Font.DemiBold; color: action.primary ? "@on-accent@" : "@ink@" }
        MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; onClicked: action.clicked() }
    }

    Column {
        id: place
        x: Math.max(48, Math.round(root.width * 0.08))
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(root.width - 2 * x, 720)
        spacing: 16

        Arrive {
            order: 0
            visible: countDown.running
            Column {
                spacing: 6
                Label {
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: "@high@"
                    text: {
                        const t = (one, many) => i18ndp("plasma_lookandfeel_org.kde.lookandfeel", one, many, root.remainingTime);
                        switch (sdtype) {
                        case ShutdownType.ShutdownTypeReboot:
                            return softwareUpdatePending ? t("Installing software updates and restarting in 1 second", "Installing software updates and restarting in %1 seconds")
                                : t("Restarting in 1 second", "Restarting in %1 seconds");
                        case ShutdownType.ShutdownTypeNone:
                            return t("Logging out in 1 second", "Logging out in %1 seconds");
                        default:
                            return softwareUpdatePending ? t("Installing software updates and shutting down in 1 second", "Installing software updates and shutting down in %1 seconds")
                                : t("Shutting down in 1 second", "Shutting down in %1 seconds");
                        }
                    }
                }
                // The window closing: a 2px line that runs out with the countdown.
                Item {
                    width: 216
                    height: 2
                    Rectangle { width: parent.width; height: 2; color: "@low@" }
                    Rectangle {
                        width: parent.width * root.remainingTime / root.timeout
                        height: 2
                        color: "@ink@"
                        Behavior on width { enabled: !root.still; NumberAnimation { duration: 1000; easing.type: Easing.Linear } }
                    }
                }
            }
        }

        Arrive {
            order: 1
            Label { text: kuser.fullName || kuser.loginName; font.pixelSize: 42; font.weight: Font.Light }
        }

        Arrive {
            order: 2
            Column {
                spacing: 4
                width: place.width
                Label {
                    width: parent.width
                    font.pixelSize: 13
                    color: "@high@"
                    visible: otherSessions.count > 0 && (sdtype !== ShutdownType.ShutdownTypeNone || root.showAllOptions)
                    text: i18ndp("plasma_lookandfeel_org.kde.lookandfeel",
                                 "One other user is currently logged in. If the computer is shut down or restarted, that user may lose work.",
                                 "%1 other users are currently logged in. If the computer is shut down or restarted, those users may lose work.",
                                 otherSessions.count)
                }
                Label {
                    width: parent.width
                    font.pixelSize: 13
                    color: "@high@"
                    visible: rebootToFirmwareSetup
                    text: isUefi ? root.tr("When restarted, the computer will enter UEFI firmware settings.")
                                 : root.tr("When restarted, the computer will enter the firmware setup screen.")
                }
                Label { width: parent.width; font.pixelSize: 13; color: "@high@"; visible: rebootToBootLoaderMenu; text: root.tr("When restarted, the computer will enter the boot loader menu.") }
                Label {
                    width: parent.width
                    font.pixelSize: 13
                    color: "@high@"
                    visible: rebootToBootLoaderEntry != ""
                    text: i18ndc("plasma_lookandfeel_org.kde.lookandfeel", "%1 is a boot loader entry", "When restarted, the computer will switch to %1.", rebootToBootLoaderEntry)
                }
            }
        }

        Arrive {
            order: 3
            Flow {
                id: actions
                width: place.width
                spacing: 8
                Keys.onLeftPressed: nextItemInFocusChain(false).forceActiveFocus()
                Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus()
                Action {
                    visible: spdMethods.SuspendState && root.showAllOptions
                    text: root.word(i18ndc("plasma_lookandfeel_org.kde.lookandfeel", "Suspend to RAM", "Slee&p"))
                    onClicked: root.sleepRequested()
                }
                Action {
                    visible: spdMethods.HibernateState && root.showAllOptions
                    text: root.word(root.tr("&Hibernate"))
                    onClicked: root.hibernateRequested()
                }
                Action {
                    id: reboot
                    visible: maysd && (sdtype === ShutdownType.ShutdownTypeReboot || root.showAllOptions)
                    primary: sdtype === ShutdownType.ShutdownTypeReboot
                    text: softwareUpdatePending ? root.word(i18ndc("plasma_lookandfeel_org.kde.lookandfeel", "@action:button Keep short", "Install Updates and Restart"))
                        : root.word(root.showAllOptions ? root.tr("&Restart") : root.tr("&Restart Now"))
                    onClicked: softwareUpdatePending ? root.rebootUpdateRequested() : root.rebootRequested()
                }
                Action {
                    visible: maysd && softwareUpdatePending && (sdtype === ShutdownType.ShutdownTypeReboot || root.showAllOptions)
                    text: root.word(root.showAllOptions ? root.tr("&Restart") : root.tr("&Restart Now"))
                    onClicked: root.rebootRequested()
                }
                Action {
                    id: halt
                    visible: maysd && (sdtype === ShutdownType.ShutdownTypeHalt || root.showAllOptions)
                    primary: sdtype === ShutdownType.ShutdownTypeHalt || (root.showAllOptions && visible)
                    text: softwareUpdatePending ? root.word(i18ndc("plasma_lookandfeel_org.kde.lookandfeel", "@action:button Keep short", "Install Updates and Shut Down"))
                        : root.word(root.showAllOptions ? root.tr("&Shut Down") : root.tr("&Shut Down Now"))
                    onClicked: softwareUpdatePending ? root.haltUpdateRequested() : root.haltRequested()
                }
                Action {
                    visible: maysd && softwareUpdatePending && (sdtype === ShutdownType.ShutdownTypeHalt || root.showAllOptions)
                    text: root.word(root.showAllOptions ? root.tr("&Shut Down") : root.tr("&Shut Down Now"))
                    onClicked: root.haltRequested()
                }
                Action {
                    id: logout
                    visible: canLogout && (sdtype === ShutdownType.ShutdownTypeNone || root.showAllOptions)
                    primary: sdtype === ShutdownType.ShutdownTypeNone || (root.showAllOptions && !halt.visible)
                    text: root.word(root.showAllOptions ? root.tr("&Log Out") : root.tr("&Log Out Now"))
                    onClicked: root.logoutRequested()
                }
                Action {
                    text: root.word(root.tr("&Cancel"))
                    onClicked: root.cancelRequested()
                }
            }
        }
    }

    // Focus starts on the action the request asked for.
    Component.onCompleted: {
        const first = sdtype === ShutdownType.ShutdownTypeReboot ? reboot
            : sdtype === ShutdownType.ShutdownTypeNone ? logout : halt;
        (first.visible ? first : logout).forceActiveFocus();
    }
}
