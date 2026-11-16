import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    function openEditor(record) {
        var dialog = pageStack.push(Qt.resolvedUrl("AlbumDialog.qml"), { record: record || {} })
        dialog.accepted.connect(function() {
            var value = dialog.values()
            if (value.id) albumController.updateAlbum(value.id, value.performer, value.album, value.year, value.genre)
            else albumController.createAlbum(value.performer, value.album, value.year, value.genre)
        })
    }

    SilicaListView {
        anchors.fill: parent
        model: albumController.albums

        PullDownMenu {
            MenuItem { text: qsTr("Добавить альбом"); onClicked: page.openEditor({}) }
            MenuItem { text: qsTr("Синхронизировать"); enabled: albumController.online && albumController.authenticated; onClicked: albumController.synchronize() }
            MenuItem { text: qsTr("О программе"); onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml")) }
        }

        header: Column {
            width: page.width
            PageHeader { title: qsTr("Музыкальная коллекция"); description: qsTr("ЛР2 · вариант 9") }
            TextField {
                id: address
                width: parent.width
                label: qsTr("Адрес HTTPS API")
                text: albumController.baseUrl
                onTextChanged: albumController.baseUrl = text
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            }
            TextSwitch { text: qsTr("Сеть включена"); checked: albumController.online; onCheckedChanged: albumController.online = checked }
            TextSwitch { text: qsTr("Разрешить тестовый сертификат"); checked: albumController.allowInsecureTls; onCheckedChanged: albumController.allowInsecureTls = checked }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium
                TextField { id: username; width: page.width * 0.38; label: qsTr("Пользователь"); text: "demo" }
                TextField { id: password; width: page.width * 0.38; label: qsTr("Пароль"); text: "demo"; echoMode: TextInput.Password }
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: albumController.authenticated ? qsTr("Обновить") : qsTr("Войти")
                enabled: !albumController.busy
                onClicked: albumController.authenticated ? albumController.refresh() : albumController.login(username.text, password.text)
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: albumController.status + (albumController.pendingCount ? qsTr(" · в очереди: %1").arg(albumController.pendingCount) : "")
                color: Theme.secondaryColor; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
            }
            Label {
                visible: text.length > 0
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                text: albumController.error; color: Theme.errorColor; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
            }
            SectionHeader { text: qsTr("Альбомы") }
        }

        delegate: ListItem {
            id: item
            contentHeight: content.height + 2 * Theme.paddingMedium
            menu: ContextMenu {
                MenuItem { text: qsTr("Изменить"); onClicked: page.openEditor(modelData) }
                MenuItem { text: qsTr("Удалить"); onClicked: remorse.execute(item, qsTr("Удаление"), function() { albumController.deleteAlbum(modelData.id) }) }
            }
            RemorseItem { id: remorse }
            Column {
                id: content
                anchors { left: parent.left; right: parent.right; margins: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                Label { text: modelData.album; color: item.highlighted ? Theme.highlightColor : Theme.primaryColor; truncationMode: TruncationMode.Fade }
                Label { text: modelData.performer + " · " + modelData.year + " · " + modelData.genre + (modelData.dirty ? qsTr(" · offline") : ""); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeSmall; truncationMode: TruncationMode.Fade }
            }
            onClicked: page.openEditor(modelData)
        }

        ViewPlaceholder { enabled: albumController.albums.length === 0; text: qsTr("Нет альбомов"); hintText: qsTr("Добавьте запись или выполните вход и обновление") }
        VerticalScrollDecorator {}
    }
}
