import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import QtQuick.Dialogs 6.3

Rectangle {
    id: root
    anchors.fill: parent
    color: "#F8FAFC"

    property bool isConnected: sshFileManager ? sshFileManager.connected : false
    property bool isBusy: sshFileManager ? sshFileManager.busy : false
    property string statusMessage: sshFileManager ? sshFileManager.statusMessage : "未连接"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // 顶部控制面板
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
    color: "#FFFFFF"
            radius: 12
            border.color: "#E2E8F0"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                // 标题和状态行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    Text {
                        text: "黑盒子文件管理"
                        font.pixelSize: 20
                        font.bold: true
                        color: "#1E293B"
                    }

                    Item { Layout.fillWidth: true }

                    // 连接状态指示器
                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: isConnected ? "#10B981" : (isBusy ? "#F59E0B" : "#EF4444")
                        
                        SequentialAnimation on opacity {
                            running: isBusy
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 500 }
                            NumberAnimation { to: 1.0; duration: 500 }
                        }
                    }

                    Text {
                        text: statusMessage
                        font.pixelSize: 12
                        color: "#64748B"
                    }
                }

                // 操作按钮行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

        Button {
            id: triggerBlackBoxButton
            text: "触发黑盒子"
                        Material.background: Material.accent
                        Material.foreground: "white"
                        enabled: !isBusy

            onClicked: {
                console.log("点击了触发黑盒子按钮！");
                if (appManager) {
                    appManager.triggerBlackBox();
                            }
                        }
                    }

                    Button {
                        text: isConnected ? "刷新文件列表" : "连接服务器"
                        enabled: !isBusy
                        Material.background: isConnected ? "#059669" : "#2563EB"
                        Material.foreground: "white"

                        onClicked: {
                            if (isConnected) {
                                sshFileManager.refreshFileList();
                } else {
                                sshFileManager.testConnection();
                            }
                        }
                    }

                    Button {
                        text: "选择全部"
                        enabled: !isBusy && isConnected && fileListView.model
                        
                        onClicked: {
                            if (fileListView.model) {
                                fileListView.model.selectAll();
                            }
                        }
                    }

                    Button {
                        text: "清除选择"
                        enabled: !isBusy && isConnected && fileListView.model
                        
                        onClicked: {
                            if (fileListView.model) {
                                fileListView.model.clearSelection();
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "下载选中文件"
                        enabled: !isBusy && isConnected && fileListView.model && 
                                fileListView.model.hasSelection
                        Material.background: "#DC2626"
                        Material.foreground: "white"

                        onClicked: {
                            sshFileManager.downloadSelectedFiles("");
                        }
                    }
                }
            }
        }

        // 文件列表区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#FFFFFF"
            radius: 12
            border.color: "#E2E8F0"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                // 列表标题
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "文件列表"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#1E293B"
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#E2E8F0"
                }

                // 文件列表
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: fileListView
                        spacing: 2

                        delegate: Rectangle {
                            width: fileListView.width
                            height: 60
                            color: mouseArea.containsMouse ? "#F1F5F9" : "transparent"
                            radius: 8

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 15

                                // 选择框
                                CheckBox {
                                    checked: model.selected || false
                                    enabled: !model.isDirectory
                                    onToggled: {
                                        if (fileListView.model) {
                                            fileListView.model.toggleSelection(index);
                                        }
                                    }
                                }

                                // 文件图标
                                Text {
                                    text: model.isDirectory ? "📁" : "📄"
                                    font.pixelSize: 24
                                }

                                // 文件信息
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: model.name || ""
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#1E293B"
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        spacing: 15

                                        Text {
                                            text: "大小: " + formatFileSize(model.size || 0)
                                            font.pixelSize: 11
                                            color: "#64748B"
                                        }

                                        Text {
                                            text: "修改时间: " + formatDateTime(model.modifiedTime)
                                            font.pixelSize: 11
                                            color: "#64748B"
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onClicked: {
                                    if (!model.isDirectory && fileListView.model) {
                                        fileListView.model.toggleSelection(index);
                                    }
                                }
                            }
                        }

                        // 空状态提示
                        Rectangle {
                            anchors.centerIn: parent
                            width: 300
                            height: 200
                            color: "transparent"
                            visible: !fileListView.model || fileListView.model.rowCount() === 0

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 15

                                Text {
                                    text: "📂"
                                    font.pixelSize: 48
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: isConnected ? "文件夹为空" : "请先连接服务器"
                                    font.pixelSize: 16
                                    color: "#64748B"
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: isConnected ? "触发黑盒子后会生成新文件" : "点击连接按钮开始"
                                    font.pixelSize: 12
                                    color: "#94A3B8"
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }
                    }
                }
            }
        }

        // 下载进度区域
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: downloadProgressColumn.visible ? 120 : 0
            color: "#FFFFFF"
            radius: 12
            border.color: "#E2E8F0"
            border.width: 1
            visible: downloadProgressColumn.visible

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 200 }
            }

            ColumnLayout {
                id: downloadProgressColumn
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10
                visible: false

                Text {
                    text: "下载进度"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#1E293B"
                }

                ProgressBar {
                    id: overallProgressBar
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 0
                }

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        id: progressText
                        text: "准备下载..."
                        font.pixelSize: 12
                        color: "#64748B"
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "取消下载"
                        Material.background: "#EF4444"
                        Material.foreground: "white"
                        onClicked: {
                            sshFileManager.cancelAllDownloads();
                        }
                    }
                }
            }
        }
    }

    // 移除不再需要的 FolderDialog

    // SSH文件管理器信号连接
    Connections {
        target: sshFileManager

        function onFileListReady(model) {
            console.log("文件列表就绪");
            fileListView.model = model;
        }

        function onFileListError(error) {
            console.log("文件列表错误:", error);
            errorDialog.errorText = error;
            errorDialog.open();
        }

        function onDownloadStarted(fileName) {
            console.log("开始下载:", fileName);
            downloadProgressColumn.visible = true;
            progressText.text = "正在下载: " + fileName;
        }

        function onDownloadProgress(fileName, bytesReceived, bytesTotal) {
            var progress = bytesTotal > 0 ? (bytesReceived * 100 / bytesTotal) : 0;
            progressText.text = "下载中: " + fileName + " (" + Math.round(progress) + "%)";
        }

        function onDownloadFinished(fileName, localPath) {
            console.log("下载完成:", fileName, "->", localPath);
        }

        function onDownloadFailed(fileName, error) {
            console.log("下载失败:", fileName, error);
            errorDialog.errorText = "下载失败: " + fileName + "\n" + error;
            errorDialog.open();
        }

        function onOverallProgress(completedFiles, totalFiles, totalBytesReceived, totalBytesExpected) {
            var progress = totalBytesExpected > 0 ? (totalBytesReceived * 100 / totalBytesExpected) : 0;
            overallProgressBar.value = progress;
            progressText.text = "总进度: " + completedFiles + "/" + totalFiles + " 文件 (" + Math.round(progress) + "%)";
        }

        function onAllDownloadsCompleted() {
            console.log("所有下载完成");
            downloadProgressColumn.visible = false;
            successDialog.open();
        }
    }

    // 错误对话框
    Dialog {
        id: errorDialog
        title: "错误"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        property string errorText: ""

        Label {
            text: errorDialog.errorText
            wrapMode: Text.Wrap
            width: 300
        }
    }

    // 成功对话框
    Dialog {
        id: successDialog
        title: "下载完成"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        Label {
            text: "所有文件下载完成！"
        }
    }

    // 辅助函数
    function formatFileSize(bytes) {
        if (bytes === 0) return "0 B";
        
        var k = 1024;
        var sizes = ["B", "KB", "MB", "GB"];
        var i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i];
    }

    function formatDateTime(dateTime) {
        if (!dateTime) return "";
        return Qt.formatDateTime(dateTime, "yyyy-MM-dd hh:mm:ss");
    }

    function getCurrentUser() {
        // 简单的用户名获取，实际可能需要更复杂的逻辑
        return "Default";
        }
}