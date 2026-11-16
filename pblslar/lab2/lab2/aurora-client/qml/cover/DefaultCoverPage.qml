import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent; width: parent.width - 2 * Theme.paddingLarge
        Label { width: parent.width; text: qsTr("Музыка"); horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.fontSizeLarge }
        Label { width: parent.width; text: qsTr("Записей: %1").arg(albumController.albums.length); horizontalAlignment: Text.AlignHCenter; color: Theme.secondaryColor }
        Label { width: parent.width; text: albumController.online ? qsTr("online") : qsTr("offline"); horizontalAlignment: Text.AlignHCenter; color: Theme.highlightColor }
    }
}
