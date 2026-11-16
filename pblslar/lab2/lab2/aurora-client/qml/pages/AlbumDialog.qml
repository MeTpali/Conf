import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    property var record: ({ id: 0, performer: "", album: "", year: "", genre: "" })
    canAccept: performer.text.length > 0 && title.text.length > 0 && year.text.length > 0 && genre.text.length > 0

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height
        Column {
            id: column
            width: parent.width
            DialogHeader { acceptText: qsTr("Сохранить"); cancelText: qsTr("Отмена") }
            TextField { id: performer; width: parent.width; label: qsTr("Исполнитель"); text: record.performer || "" }
            TextField { id: title; width: parent.width; label: qsTr("Альбом"); text: record.album || "" }
            TextField { id: year; width: parent.width; label: qsTr("Год"); text: record.year || ""; inputMethodHints: Qt.ImhDigitsOnly }
            TextField { id: genre; width: parent.width; label: qsTr("Жанр"); text: record.genre || "" }
        }
    }

    function values() { return { id: record.id || 0, performer: performer.text, album: title.text, year: year.text, genre: genre.text } }
}
