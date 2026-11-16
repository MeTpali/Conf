import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: aboutColumn.height + Theme.paddingLarge

        Column {
            id: aboutColumn
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("О программе") }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Преобразователь температуры")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Лабораторная работа № 2. Вариант 5 для номера в списке 19. Клиент Qt/C++ для ОС Аврора обменивается данными с сервером по WebSocket.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }
        }
    }
}

