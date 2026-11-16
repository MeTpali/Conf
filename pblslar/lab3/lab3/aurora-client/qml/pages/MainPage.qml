import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    SilicaListView {
        anchors.fill: parent
        model: resilientController.notifications

        PullDownMenu {
            MenuItem {
                text: qsTr("Добавить тестовый альбом")
                onClicked: resilientController.addTestAlbum()
            }
            MenuItem {
                text: qsTr("Проверить сейчас")
                onClicked: resilientController.checkNow()
            }
            MenuItem {
                text: qsTr("Запустить мониторинг")
                onClicked: resilientController.start()
            }
            MenuItem {
                text: qsTr("Остановить")
                onClicked: resilientController.stop()
            }
        }

        header: Column {
            width: page.width

            PageHeader {
                title: qsTr("Отказоустойчивый клиент")
                description: qsTr("ЛР3 · вариант 9")
            }

            TextField {
                width: parent.width
                label: qsTr("Адрес балансировщика")
                text: resilientController.apiUrl
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onTextChanged: resilientController.apiUrl = text
            }

            TextField {
                width: parent.width
                label: qsTr("WebSocket push")
                text: resilientController.webSocketUrl
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onTextChanged: resilientController.webSocketUrl = text
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Подключиться")
                onClicked: resilientController.start()
            }

            SectionHeader { text: qsTr("Состояние") }

            DetailItem {
                label: qsTr("Backend")
                value: resilientController.available ? qsTr("доступен") : qsTr("недоступен")
            }
            DetailItem {
                label: qsTr("Экземпляр")
                value: resilientController.instance || "—"
            }
            DetailItem {
                label: qsTr("Версия")
                value: resilientController.version || "—"
            }
            DetailItem {
                label: qsTr("Push")
                value: resilientController.pushConnected ? qsTr("подключён") : qsTr("нет соединения")
            }
            DetailItem {
                label: qsTr("Повторные попытки")
                value: String(resilientController.retryCount)
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: resilientController.status
                color: Theme.highlightColor
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            SectionHeader { text: qsTr("Альбомы через балансировщик") }

            DetailItem {
                label: qsTr("Ответил экземпляр")
                value: resilientController.albumsInstance || "—"
            }

            Repeater {
                model: resilientController.albums
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: modelData
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            SectionHeader { text: qsTr("Push-уведомления") }
        }

        delegate: BackgroundItem {
            width: page.width
            height: message.height + 2 * Theme.paddingMedium

            Label {
                id: message
                anchors {
                    left: parent.left
                    right: parent.right
                    margins: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: modelData
                wrapMode: Text.Wrap
                color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }

        ViewPlaceholder {
            enabled: resilientController.notifications.length === 0
            text: qsTr("Уведомлений пока нет")
        }

        VerticalScrollDecorator {}
    }

    Component.onCompleted: resilientController.start()
}
