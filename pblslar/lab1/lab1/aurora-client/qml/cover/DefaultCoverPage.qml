import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        width: parent.width - 2 * Theme.paddingLarge
        anchors.centerIn: parent
        spacing: Theme.paddingMedium

        Label {
            width: parent.width
            text: qsTr("Температура")
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Label {
            width: parent.width
            text: temperatureController.result
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}

