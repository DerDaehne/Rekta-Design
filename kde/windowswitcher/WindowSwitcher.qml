/*
    Alt+Tab, in Rekta: a floating surface (the Plasma Style's popup: the ground, a 1px edge, no shadow) with the windows
    as pictures on a strict grid. Each picture has a hairline edge; the chosen one is edged in ink, 2px, and its caption
    turns from grey to ink. Content over chrome: no box behind the chosen one.

    scripts/build-kde.mjs fills in the colours (the @…@ marks) from the tokens.
*/
import QtQuick
import org.kde.kwin as KWin
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami

KWin.TabBoxSwitcher {
    id: tabBox

    Instantiator {
        active: tabBox.visible
        delegate: PlasmaCore.Dialog {
            location: PlasmaCore.Types.Floating
            visible: true
            flags: Qt.Popup | Qt.X11BypassWindowManagerHint
            x: tabBox.screenGeometry.x + Math.round((tabBox.screenGeometry.width - main.width) / 2)
            y: tabBox.screenGeometry.y + Math.round((tabBox.screenGeometry.height - main.height) / 2)

            mainItem: FocusScope {
                id: main
                focus: true
                // A column count that fits 90% of the screen; rows as many as needed, up to 70% of its height.
                readonly property int columns: Math.max(1, Math.min(grid.count, Math.floor(tabBox.screenGeometry.width * 0.9 / grid.cellWidth)))
                readonly property int rows: Math.max(1, Math.ceil(grid.count / columns))
                width: columns * grid.cellWidth
                height: Math.min(rows * grid.cellHeight, Math.floor(tabBox.screenGeometry.height * 0.7))

                GridView {
                    id: grid
                    anchors.fill: parent
                    focus: true
                    model: tabBox.model
                    currentIndex: tabBox.currentIndex
                    keyNavigationWraps: true
                    highlightMoveDuration: 0
                    readonly property int pad: Kirigami.Units.largeSpacing
                    readonly property int thumbWidth: Kirigami.Units.gridUnit * 14
                    readonly property int thumbHeight: Math.round(thumbWidth * tabBox.screenGeometry.height / tabBox.screenGeometry.width)
                    readonly property int captionHeight: Kirigami.Units.gridUnit * 2
                    cellWidth: thumbWidth + 2 * pad
                    cellHeight: thumbHeight + captionHeight + 2 * pad
                    onCurrentIndexChanged: tabBox.currentIndex = currentIndex

                    delegate: MouseArea {
                        id: cell
                        required property int index
                        required property var windowId
                        required property string caption
                        required property var icon
                        readonly property bool current: GridView.isCurrentItem
                        width: grid.cellWidth
                        height: grid.cellHeight
                        hoverEnabled: true
                        Accessible.name: caption
                        Accessible.role: Accessible.ListItem
                        onClicked: tabBox.model.activate(index)

                        Item {
                            id: picture
                            x: grid.pad
                            y: grid.pad
                            width: grid.thumbWidth
                            height: grid.thumbHeight
                            KWin.WindowThumbnail { anchors.fill: parent; anchors.margins: 2; wId: cell.windowId }
                            // The hairline edge; 2px in ink for the chosen window, the hover overlay under the pointer.
                            Rectangle { anchors.fill: parent; color: "@hover@"; opacity: cell.containsMouse && !cell.current ? @hoverOpacity@ : 0 }
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.width: cell.current ? 2 : 1
                                border.color: cell.current ? "@ink@" : "@low@"
                            }
                        }
                        Row {
                            x: grid.pad
                            anchors.top: picture.bottom
                            height: grid.captionHeight
                            width: grid.thumbWidth
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Kirigami.Units.iconSizes.small
                                height: width
                                source: cell.icon
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - Kirigami.Units.iconSizes.small - parent.spacing
                                text: cell.caption
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                                font.family: Kirigami.Theme.defaultFont.family
                                font.pixelSize: 13
                                font.weight: cell.current ? Font.DemiBold : Font.Normal
                                color: cell.current ? "@ink@" : "@high@"
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: grid.count === 0
                    text: i18ndc("kwin", "@info:placeholder no entries in the task switcher", "No open windows")
                    font.family: Kirigami.Theme.defaultFont.family
                    color: "@high@"
                }

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Left) grid.moveCurrentIndexLeft();
                    else if (event.key === Qt.Key_Right) grid.moveCurrentIndexRight();
                    else if (event.key === Qt.Key_Up) grid.moveCurrentIndexUp();
                    else if (event.key === Qt.Key_Down) grid.moveCurrentIndexDown();
                    else return;
                    tabBox.currentIndex = grid.currentIndex;
                }
            }
            onSceneGraphError: () => {}   // a graphics reset must not take KWin down
        }
    }
}
