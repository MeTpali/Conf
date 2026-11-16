import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height
        Column {
            id: column; width: parent.width; spacing: Theme.paddingLarge
            PageHeader { title: qsTr("О программе") }
            Label { width: parent.width - 2 * Theme.horizontalPageMargin; anchors.horizontalCenter: parent.horizontalCenter; wrapMode: Text.Wrap; text: qsTr("ЛР2. HTTPS REST API и offline-клиент музыкальной коллекции. Qt, QML, C++, QNetworkAccessManager, QSettings, cookie и SQLite.") }
        }
    }
}
