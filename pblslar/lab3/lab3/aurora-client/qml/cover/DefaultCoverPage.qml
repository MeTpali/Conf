import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent; width: parent.width - 2 * Theme.paddingLarge
        Label { width: parent.width; text: qsTr("Music HA"); horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.fontSizeLarge }
        Label { width: parent.width; text: resilientController.available ? qsTr("доступен") : qsTr("недоступен"); horizontalAlignment: Text.AlignHCenter; color: resilientController.available ? Theme.highlightColor : Theme.errorColor }
        Label { width: parent.width; text: resilientController.instance; horizontalAlignment: Text.AlignHCenter; color: Theme.secondaryColor }
    }
}
