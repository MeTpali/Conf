import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "mainPage"
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("О программе")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Очистить историю")
                enabled: temperatureController.connected
                onClicked: temperatureController.clearHistory()
            }
        }

        VerticalScrollDecorator {}

        Column {
            id: contentColumn
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Температура")
                description: qsTr("Вариант 5")
            }

            SectionHeader { text: qsTr("Подключение к серверу") }

            TextField {
                id: addressField
                width: parent.width
                text: "ws://192.168.1.10:8080/ws"
                label: qsTr("WebSocket-адрес")
                placeholderText: qsTr("ws://192.168.1.10:8080/ws")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: connectButton.clicked()
            }

            Button {
                id: connectButton
                anchors.horizontalCenter: parent.horizontalCenter
                text: temperatureController.connected || temperatureController.connecting
                      ? qsTr("Отключиться") : qsTr("Подключиться")
                onClicked: {
                    if (temperatureController.connected || temperatureController.connecting)
                        temperatureController.disconnectFromServer()
                    else
                        temperatureController.connectToServer(addressField.text)
                }
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: temperatureController.status
                color: temperatureController.connected
                       ? Theme.highlightColor : Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            SectionHeader { text: qsTr("Преобразование") }

            TextField {
                id: valueField
                width: parent.width
                label: qsTr("Исходная температура")
                placeholderText: qsTr("Например, 36,6")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: convertButton.clicked()
            }

            ComboBox {
                id: unitBox
                width: parent.width
                label: qsTr("Из")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Градусы Цельсия (°C)") }
                    MenuItem { text: qsTr("Градусы Фаренгейта (°F)") }
                }
            }

            Button {
                id: convertButton
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Преобразовать")
                enabled: temperatureController.connected
                onClicked: {
                    var unit = unitBox.currentIndex === 0 ? "C" : "F"
                    temperatureController.convert(valueField.text, unit)
                }
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: temperatureController.result
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                visible: text.length > 0
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: temperatureController.error
                color: Theme.errorColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            SectionHeader {
                text: qsTr("История")
                visible: temperatureController.history.length > 0
            }

            Repeater {
                model: temperatureController.history
                delegate: BackgroundItem {
                    width: page.width
                    height: historyLabel.height + 2 * Theme.paddingMedium

                    Label {
                        id: historyLabel
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        text: modelData
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}

