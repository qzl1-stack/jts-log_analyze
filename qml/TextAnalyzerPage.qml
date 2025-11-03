import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import QtQuick.Dialogs 6.3

Page {
    id: textAnalyzerPageRoot

    property string searchText: ""
    property string fileContent: ""
    property bool isSearching: false
    property bool searchResultsReady: false // 标记搜索结果是否准备好

    // 性能优化：缓存搜索结果
    property var cachedSearchResults: []
    property string lastSearchText: ""
    property string cachedHighlightedContent: "" // 缓存高亮的文本内容
    property string formattedFileContent: "" // 新增：缓存格式化后的文件内容
    property string startTime: "" // 文本开始时间
    property string endTime: "" // 文本结束时间

    // 文件列表相关属性
    property bool hasFileList: false // 是否有文件列表
    property var fileListModel: null // 文件列表模型
    property string currentFilePath: "" // 当前选中的文件路径
    property bool isLoadingFile: false // 是否正在加载文件

    // 顶部工具栏
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 80
        color: "#FFFFFF"

        // 添加阴影效果
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: parent.height
            height: 4
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#20000000" }
                GradientStop { position: 1.0; color: "#00000000" }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            // // 标题
            // Text {
            //     text: "车辆分析器"
            //     font.pixelSize: 24
            //     font.bold: true
            //     color: Material.accent
            //     Layout.alignment: Qt.AlignVCenter
            // }

            Item { Layout.fillWidth: true } // 弹性占位符

            // 文件选择下拉框容器
            Rectangle {
                id: fileSelector
                Layout.preferredWidth: 280
                Layout.preferredHeight: 40
                color: "#F8FAFC"
                border.color: "#E2E8F0"
                border.width: 1
                radius: 8
                visible: hasFileList
                Layout.alignment: Qt.AlignVCenter

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // 文件图标
                    Text {
                        text: "📁"
                        font.pixelSize: 16
                        color: "#64748B"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // 文件选择下拉框
                    ComboBox {
                        id: fileComboBox
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        model: fileListModel
                        textRole: "name"
                        valueRole: "path"

                        delegate: ItemDelegate {
                            width: fileComboBox.width
                            height: 50

                            Rectangle {
                                anchors.fill: parent
                                color: parent.hovered ? "#EFF6FF" : "transparent"
                                radius: 4

                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 10
                                    spacing: 2

                                    Row {
                                        spacing: 8

                                        // 类别标签
                                        Rectangle {
                                            width: categoryLabel.width + 8
                                            height: 16
                                            radius: 8
                                            color: getCategoryColor(model.category)

                                            Text {
                                                id: categoryLabel
                                                anchors.centerIn: parent
                                                text: model.category || ""
                                                font.pixelSize: 9
                                                color: "white"
                                                font.bold: true
                                            }
                                        }

                                        Text {
                                            text: model.name || ""
                                            font.pixelSize: 12
                                            color: "#1E293B"
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        text: "大小: " + formatFileSize(model.size || 0) + " | 关键字: " + (model.keyword || "")
                                        font.pixelSize: 10
                                        color: "#64748B"
                                        width: parent.width
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        onCurrentIndexChanged: {
                            if (currentIndex >= 0 && fileListModel) {
                                var selectedFile = fileListModel.getFile(currentIndex)
                                if (selectedFile && selectedFile.path !== currentFilePath) {
                                    currentFilePath = selectedFile.path
                                    loadSelectedFile(selectedFile.path)
                                }
                            }
                        }
                        contentItem: Text {
                            text: fileComboBox.displayText
                            font.pixelSize: 12
                            color: "#1E293B"
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 5
                        }

                        background: Rectangle {
                            color: "transparent"
                            border.color: "transparent"
                        }
                    }
                }
            }

            // 刷新按钮
            Rectangle {
                id: refreshButton
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                color: refreshMouseArea.pressed ? "#D1D5DB" : (refreshMouseArea.containsMouse ? "#E5E7EB" : "transparent")
                radius: 8
                Layout.alignment: Qt.AlignVCenter

                Behavior on color { ColorAnimation { duration: 150 } }

                Text {
                    text: "🔄"
                    font.pixelSize: 18
                    anchors.centerIn: parent
                    color: "#374151"
                }

                MouseArea {
                    id: refreshMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        console.log("刷新按钮被点击")
                        // fileHandler.clearFileCache() // 保留此行
                        sqliteTextHandler.clearDatabase()
                        if (currentFilePath.length > 0) {
                            loadSelectedFile(currentFilePath)
                        }
                    }
                }

                ToolTip.visible: refreshMouseArea.containsMouse
                ToolTip.text: "刷新文件"
            }

            // 时间范围显示容器
            Rectangle {
                Layout.preferredWidth: 320
                Layout.preferredHeight: 40
                color: "#F8FAFC"
                border.color: "#E2E8F0"
                border.width: 1
                radius: 8
                visible: startTime.length > 0 && endTime.length > 0
                Layout.alignment: Qt.AlignVCenter

                Row {
                    anchors.centerIn: parent
                    spacing: 10

                    // 时钟图标
                    Text {
                        text: "🕐"
                        font.pixelSize: 16
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Column {
                        spacing: 2
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            text: "时间范围"
                            font.pixelSize: 10
                            color: "#64748B"
                            font.bold: true
                        }

                        Row {
                            spacing: 6

                            Text {
                                text: startTime
                                font.pixelSize: 11
                                font.family: "Consolas, Monaco, monospace"
                                color: "#059669"
                                font.bold: true
                            }

                            Text {
                                text: "→"
                                font.pixelSize: 12
                                color: "#64748B"
                            }

                            Text {
                                text: endTime
                                font.pixelSize: 11
                                font.family: "Consolas, Monaco, monospace"
                                color: "#DC2626"
                                font.bold: true
                            }
                        }
                    }
                }
            }

            // 搜索框容器
            Rectangle {
                Layout.preferredWidth: 400
                Layout.preferredHeight: 40
                color: "#F8FAFC"
                border.color: searchInput.activeFocus ? Material.accent : "#E2E8F0"
                border.width: 2
                radius: 8
                Layout.alignment: Qt.AlignVCenter

                Behavior on border.color {
                    ColorAnimation { duration: 200 }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // 搜索图标
                    Rectangle {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        color: "transparent"
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            anchors.centerIn: parent
                            text: "🔍"
                            font.pixelSize: 16
                            color: "#64748B"
                        }
                    }

                    // 搜索输入框
                    TextField {
                        id: searchInput
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        font.pixelSize: 14
                        color: "#1E293B"
                        placeholderText: text.length === 0 ? "输入关键词搜索..." : ""
                        Layout.alignment: Qt.AlignVCenter

                        background: Rectangle {
                            color: "transparent"
                        }

                        onTextChanged: {
                            searchText = text
                            if (text.length > 0) {
                                searchTimer.restart()
                                searchResultsReady = false
                            } else {
                                searchTimer.stop()
                                if (textDisplay.text !== formattedFileContent) {
                                    textDisplay.text = formattedFileContent
                                }
                                resultsModel.clear()
                                searchResultsReady = false
                                cachedSearchResults = []
                                lastSearchText = ""
                                cachedHighlightedContent = ""
                                extractTimeRange()
                            }
                        }

                        Keys.onReturnPressed: {
                            searchTimer.stop()
                            performSearch()
                        }
                    }

                    // 清除按钮
                    Button {
                        text: "✕"
                        visible: searchInput.text.length > 0
                        onClicked: {
                            searchInput.text = ""
                            searchInput.forceActiveFocus()
                        }
                    }
                }
            }

            // 文件操作按钮
            Button {
                text: "加载文件"
                Layout.preferredWidth: 120
                Layout.preferredHeight: 40
                Layout.alignment: Qt.AlignVCenter

                onClicked: {
                    fileDialog.open()
                }
            }
        } // End of main RowLayout
    }

    // 主内容区域
    Rectangle {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        color: "#FFFFFF"

        // 侧边栏（搜索结果）
        Rectangle {
            id: sidebar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            // width: (searchText.length > 0 && searchResultsReady && resultsModel.count > 0) ? 300 : 0
            width: 300
            color: "#F8FAFC"
            border.color: "#E2E8F0"
            border.width: width > 0 ? 1 : 0

            // // 启用硬件加速
            // layer.enabled: true
            // layer.effect: ShaderEffect { /* 可选的自定义着色器 */ }

            // // 优化动画性能
            // Behavior on width {
            // NumberAnimation {
            // duration: 150
            // easing.type: Easing.Linear

            // // 添加简单的性能日志
            // onRunningChanged: {
            // if (running) {
            // console.time("SidebarAnimation")
            // } else {
            // console.timeEnd("SidebarAnimation")
            // }
            // }
            // }
            // }

            // visible: width > 0

            Column {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                // 添加搜索状态指示
                Row {
                    width: parent.width
                    spacing: 10

                    Text {
                        text: "搜索结果"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#1E293B"
                    }

                    // 搜索进度指示器（使用 BusyIndicator，避免旋转影响父项）
                    BusyIndicator {
                        running: isSearching
                        visible: isSearching
                        width: 30
                        height: 30
                    }

                    Text {
                        text: resultsModel.count > 0 ? "(" + resultsModel.count + ")" : ""
                        font.pixelSize: 12
                        color: "#64748B"
                        visible: !isSearching
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#E2E8F0"
                }

                ScrollView {
                    width: parent.width
                    height: parent.height - 60 // 调整高度以适应新的标题行

                    ListView {
                        id: searchResults
                        model: ListModel {
                            id: resultsModel
                        }

                        // 启用缓存以提高性能
                        cacheBuffer: 1000

                        delegate: Rectangle {
                            width: searchResults.width
                            height: 60
                            color: {
                                if (resultMouseArea.containsMouse) return "#EFF6FF"
                                return "transparent"
                            }
                            radius: 6

                            // 添加边框效果
                            border.width: resultMouseArea.containsMouse ? 1 : 0
                            border.color: resultMouseArea.containsMouse ? "#DBEAFE" : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: 300
                                    easing.type: Easing.InOutQuad
                                }
                            }

                            Behavior on border.color {
                                ColorAnimation { duration: 300 }
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.topMargin: 2
                                anchors.leftMargin: 2
                                radius: parent.radius
                                color: "#08000000"
                                visible: resultMouseArea.containsMouse
                                z: -1

                                Behavior on visible {
                                    NumberAnimation { duration: 300 }
                                }
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 10

                                Text {
                                    text: "第 " + (model.lineNumber || 0) + " 行"
                                    font.pixelSize: 12
                                    color: "#2563EB"
                                    font.bold: resultMouseArea.containsMouse

                                    Behavior on color {
                                        ColorAnimation { duration: 300 }
                                    }
                                }

                                Text {
                                    text: model.preview || ""
                                    font.pixelSize: 11
                                    color: resultMouseArea.containsMouse ? "#1E293B" : "#64748B"
                                    width: parent.width
                                    elide: Text.ElideRight
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2

                                    Behavior on color {
                                        ColorAnimation { duration: 300 }
                                    }
                                }
                            }

                            MouseArea {
                                id: resultMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onClicked: {
                                    textAnalyzerPageRoot.jumpToLine(model.lineNumber || 0)
                                }
                            }
                        }
                    }
                }
            }
        }

        // 文本显示区域
        Rectangle {
            anchors.top: parent.top
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            color: "#FFFFFF"

            // 鼠标滚轮缩放区域
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton // 只处理滚轮事件
                onWheel: (wheel) => {
                    if (wheel.modifiers & Qt.ControlModifier) {
                        var newSize;
                        if (wheel.angleDelta.y > 0) {
                            // 放大, 上限 40px
                            newSize = Math.min(40, textDisplay.font.pixelSize + 1);
                        } else {
                            // 缩小, 下限 8px
                            newSize = Math.max(8, textDisplay.font.pixelSize - 1);
                        }

                        // 同步更新文本和行号的字体大小
                        textDisplay.font.pixelSize = newSize;
                        lineNumberArea.font.pixelSize = newSize;

                        wheel.accepted = true; // 消费事件，防止页面滚动
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 0

                // 行号显示区域
                ScrollView {
                    id: lineNumberScrollView
                    Layout.preferredWidth: 80
                    Layout.fillHeight: true
                    clip: true

                    // 隐藏滚动条，只显示内容
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    // 添加拦截滚轮事件的 MouseArea
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton

                        // 完全拦截滚轮事件
                        onWheel: (wheel) => {
                            wheel.accepted = true // 标记事件已处理，阻止传播
                        }
                    }

                    TextArea {
                        id: lineNumberArea
                        readOnly: true
                        color: "#888888"
                        font.pixelSize: 14
                        font.family: "Consolas, Monaco, monospace"
                        background: Rectangle { color: "#F8FAFC" }
                        selectByMouse: false
                        textFormat: Text.RichText // 使用富文本格式

                        // 禁止鼠标交互
                        // mouseSelectionMode: TextInput.NoSelection
                        activeFocusOnPress: false

                        // 禁用输入和编辑
                        inputMethodHints: Qt.ImhNoPredictiveText

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            propagateComposedEvents: false
                        }
                    }
                }

                // 文本内容显示区域
                ScrollView {
                    id: textScrollView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    // 明确指定滚动条在右侧
                    ScrollBar.vertical: ScrollBar {
                        id: mainScrollBar
                        interactive: true
                        anchors.right: parent.right // 将滚动条锚定在右侧
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        // 当滚动条位置改变时，同步行号区域
                        onPositionChanged: {
                            lineNumberScrollView.ScrollBar.vertical.position = position
                        }
                    }
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                    TextArea {
                        id: textDisplay
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextArea.NoWrap
                        font.family: "Consolas, Monaco, monospace"
                        font.pixelSize: 14
                        color: "#1E293B"
                        textFormat: Text.RichText
                        background: Rectangle { color: "transparent" }

                        // 当文本变化时，更新行号
                        onTextChanged: updateLineNumbers()

                        property var searchResults: []
                    }
                }
            }

            // 空状态提示
            Column {
                anchors.centerIn: parent
                spacing: 20
                visible: fileContent.length === 0

                Text {
                    text: hasFileList ? "📂" : "📄"
                    font.pixelSize: 64
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: hasFileList ? "请选择文件" : "暂无文本内容"
                    font.pixelSize: 18
                    color: "#64748B"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: hasFileList ? "从上方下拉框中选择要查看的文件" : "点击\"加载文件\"按钮来导入文本文件或ZIP压缩包"
                    font.pixelSize: 14
                    color: "#94A3B8"
                    anchors.horizontalCenter: parent.horizontalCenter
                    wrapMode: Text.WordWrap
                    width: 400
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // 底部状态栏
    Rectangle {
        id: statusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
        color: "#F8FAFC"
        border.color: "#E2E8F0"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 15

            Text {
                text: {
                    if (hasFileList && currentFilePath.length > 0) {
                        var fileName = currentFilePath.split('/').pop()
                        return "当前文件: " + fileName + " | 字符数: " + fileContent.length
                    } else if (fileContent.length > 0) {
                        return "文件已加载 | 字符数: " + fileContent.length
                    } else {
                        return "就绪"
                    }
                }
                font.pixelSize: 12
                color: "#64748B"
            }

            Item { Layout.fillWidth: true }

            // 文件列表状态
            Text {
                text: hasFileList ? "文件列表: " + (fileListModel ? fileListModel.rowCount() : 0) + " 个文件" : ""
                font.pixelSize: 12
                color: "#059669"
                visible: hasFileList
            }

            Text {
                text: searchText.length > 0 ? "搜索: \"" + searchText + "\"" : ""
                font.pixelSize: 12
                color: Material.accent
                visible: searchText.length > 0
            }

            // 加载进度指示器
            ProgressBar {
                id: loadingIndicator
                Layout.preferredWidth: 200
                Layout.alignment: Qt.AlignVCenter
                visible: false
                from: 0
                to: 100
                value: 0
            }
        }
    }

    // 搜索延迟定时器 - 优化防抖
    Timer {
        id: searchTimer
        interval: 800 // 增加到 800ms，大幅减少搜索频率
        onTriggered: performSearch()
    }

    // 行高亮定时器
    Timer {
        id: highlightTimer
        interval: 100
        property int targetLine: 0

        onTriggered: {
            // 添加临时高亮效果
            if (targetLine > 0) {
                // 这里可以添加高亮逻辑，比如临时改变目标行的背景色
                console.log("跳转到第", targetLine, "行")
            }
        }
    }

    // 高性能多线程搜索功能
    function performSearch() {
        console.log("performSearch 被调用，搜索词:", searchText, "文件内容长度:", fileContent.length)

        if (searchText.length === 0 || fileContent.length === 0) {
            textDisplay.text = textAnalyzerPageRoot.formatForRichText(fileContent)
            resultsModel.clear()
            searchResultsReady = false
            updateLineNumbers()
            return
        }

        // 检查缓存
        if (searchText === lastSearchText && cachedSearchResults.length > 0) {
            console.log("使用缓存结果")
            // 使用缓存结果
            displayCachedResults()
            return
        }

        // 防止重复搜索
        if (isSearching) {
            console.log("取消当前搜索")
            // 取消当前搜索
            // fileHandler.cancelSearch() // 保留此行
            sqliteTextHandler.cancelSearch()
        }

        console.log("开始新的搜索")
        isSearching = true
        searchResultsReady = false
        resultsModel.clear()

        // 启动多线程搜索
        console.log("调用 fileHandler.startAsyncSearch")
        // fileHandler.startAsyncSearch(fileContent, searchText, 100) // 保留此行
        sqliteTextHandler.startAsyncSearch("", searchText, 100)
    }

    // 显示缓存结果
    function displayCachedResults() {
        resultsModel.clear()

        // 创建高亮正则表达式
        var highlightRegex = new RegExp(searchText, 'gi');

        for (var i = 0; i < cachedSearchResults.length; i++) {
            var result = cachedSearchResults[i]

            // 为预览文本添加高亮
            var highlightedPreview = result.preview.replace(
                        highlightRegex,
                        '<span style="background-color: #DBEAFE; color: #1D4ED8; font-weight: bold;">$&</span>'
                        );

            resultsModel.append({
                                    lineNumber: result.lineNumber,
                                    preview: highlightedPreview
                                })
        }

        // 使用缓存的高亮内容更新文本显示
        if (cachedHighlightedContent.length > 0) {
            textDisplay.text = cachedHighlightedContent
        } else {
            // 如果没有缓存的高亮内容，重新生成高亮
            var highlightRegex = new RegExp(searchText, 'gi');
            var highlightedContent = textAnalyzerPageRoot.formatForRichText(fileContent, false);
            highlightedContent = highlightedContent.replace(
                        highlightRegex,
                        '<span style="background-color: #DBEAFE; color: #1D4ED8; font-weight: bold;">$&</span>'
                        );
            textDisplay.text = highlightedContent;
        }

        updateLineNumbers()
        searchResultsReady = true
    }

    function jumpToLine(lineNumber) {
        if (lineNumber <= 0) {
            return;
        }

        // 获取原始文件的行数
        var originalLines = fileContent.split('\n');
        var totalOriginalLines = originalLines.length;

        if (lineNumber > totalOriginalLines) {
            return;
        }

        console.log("跳转到行:", lineNumber, "总行数:", totalOriginalLines);

        // 方法1：直接使用TextArea的positionAt方法（如果可用）
        // 首先尝试使用更精确的方法
        try {
            // 计算目标行在显示文本中的大致位置
            var displayLines = textDisplay.text.split('<br>');
            var targetDisplayLine = Math.min(lineNumber - 1, displayLines.length - 1);

            // 计算到目标行的字符数（在显示文本中）
            var displayPosition = 0;
            for (var i = 0; i < targetDisplayLine; i++) {
                displayPosition += displayLines[i].length + 4; // +4 for '<br>'
            }

            console.log("显示文本中的位置:", displayPosition);

            // 设置光标位置
            textDisplay.cursorPosition = displayPosition;
            textDisplay.forceActiveFocus();

        } catch (e) {
            console.log("使用备用方法:", e);
            // 备用方法：使用原始文件内容计算
            var targetPosition = 0;
            for (var i = 0; i < lineNumber - 1 && i < originalLines.length; i++) {
                targetPosition += originalLines[i].length + 1; // +1 for newline
            }

            // 由于显示文本可能包含HTML标签，我们需要调整位置
            // 简单的调整：假设每行平均增加了一些HTML字符
            var adjustedPosition = targetPosition * 1.1; // 增加10%的缓冲

            textDisplay.cursorPosition = Math.min(adjustedPosition, textDisplay.text.length);
            textDisplay.forceActiveFocus();
        }

        // 方法2：使用更精确的滚动条位置计算
        var scrollRatio = totalOriginalLines > 1 ? (lineNumber - 1) / (totalOriginalLines - 1) : 0;

        // 应用到滚动条
        mainScrollBar.position = scrollRatio;
        lineNumberScrollView.ScrollBar.vertical.position = scrollRatio;

        console.log("滚动比例:", scrollRatio);

        // 方法3：使用QML的内置方法确保文本可见
        Qt.callLater(function() {
            // 确保光标位置可见
            textDisplay.ensureVisible(textDisplay.cursorPosition);

            // 触发当前行高亮
            highlightTimer.targetLine = lineNumber;
            highlightTimer.restart();

            console.log("最终光标位置:", textDisplay.cursorPosition);
        });
    }

    function formatForRichText(plainText) {
        if (!plainText) return "";
        var lines = plainText.split('\n');
        var richText = lines.map(function(line) {
            var escapedLine = line.replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;");
            // 使用 &nbsp; 保证空行高度和边框可见
            if (escapedLine.trim() === "") {
                escapedLine = "&nbsp;";
            }
            // 使用 <p> 标签来设置行间距和底部分隔线
            return '<p style="margin: 0; padding: 4px 0; line-height: 1.5; border-bottom: 1px solid #F3F4F6;">' + escapedLine + '</p>';
        }).join('');
        return richText;
    }

    // 错误对话框
    Dialog {
        id: errorDialog
        title: "文件加载错误"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        property string errorText: ""

        Label {
            anchors.fill: parent
            text: errorDialog.errorText
            wrapMode: Text.Wrap
        }
    }

    // 文件对话框
    FileDialog {
        id: fileDialog
        title: "选择文本文件或ZIP压缩包"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "支持的文件 (*.txt *.md *.csv *.zip)",
            "文本文件 (*.txt *.md *.csv)",
            "压缩文件 (*.zip)",
            "所有文件 (*)"
        ]

        // 强制使用非原生对话框以确保过滤器正常工作
        options: FileDialog.DontUseNativeDialog

        onAccepted: function() {
            // 处理文件选择
            if (typeof sqliteTextHandler !== 'undefined') {
                console.log("选择的文件:", selectedFile)
                // 显示加载进度
                loadingIndicator.value = 0
                loadingIndicator.visible = true

                // 异步加载文件
                // fileHandler.loadTextFileAsync(selectedFile) // 保留此行
                sqliteTextHandler.loadTextFileAsync(selectedFile)
            }
        }

        onRejected: function() {
            console.log("用户取消了文件选择")
        }
    }

    // 文件加载信号处理
    Connections {
        // target: fileHandler
        target: sqliteTextHandler

        function onLoadProgress(progress) {
            loadingIndicator.value = progress
        }

        function onLoadError(errorMessage) {
            loadingIndicator.visible = false
            errorDialog.errorText = errorMessage
            errorDialog.open()
        }

        // 多线程搜索信号处理
        function onSearchProgress(progress) {
            // 可以在这里显示搜索进度
            console.log("搜索进度:", progress + "%")
        }

        function onSearchResultReady(results, highlightedContent) {
            // 清空现有结果
            resultsModel.clear()

            // 添加新结果
            for (var i = 0; i < results.length; i++) {
                var result = results[i]
                resultsModel.append({
                lineNumber: result.lineNumber,
                preview: result.preview
                })
            }

            // 更新显示内容（搜索结果使用富文本）
            textDisplay.textFormat = Text.RichText
            textDisplay.text = highlightedContent
            textDisplay.searchResults = results

            // 缓存结果和高亮内容
            cachedSearchResults = results
            lastSearchText = searchText
            cachedHighlightedContent = highlightedContent // 缓存高亮内容

            updateLineNumbers()
            searchResultsReady = true
        }

        function onSearchFinished() {
            isSearching = false
            console.log("搜索完成")
        }

        function onSearchCancelled() {
            isSearching = false
            searchResultsReady = false
            console.log("搜索已取消")
            // 如果取消时搜索框为空，恢复原始内容
            if (searchInput.text.length === 0) {
                textDisplay.text = formattedFileContent
                extractTimeRange() // 确保时间范围正确显示
            }
        }

        // 处理文件列表就绪信号
        function onFileListReady(model) {
            console.log("文件列表就绪")
            console.log("model:", model)
            fileListModel = model
            hasFileList = true

            // 自动选择第一个文件
            if (model && model.rowCount() > 0) {
                Qt.callLater(function() {
                    fileComboBox.currentIndex = 0
                    var firstFile = model.getFile(0)
                    if (firstFile) {
                        currentFilePath = firstFile.path
                        loadSelectedFile(firstFile.path)
                    }
                })
            }
        }

        // function onFileContentReady(content, filePath) {
        //     console.log("文件内容就绪:", filePath)
        //     isLoadingFile = false
        //     loadingIndicator.visible = false

        //     if (content.length > 0) {
        //         fileContent = content

        //         // 先用纯文本快速显示，避免大文本富文本同步格式化卡主UI
        //         // textDisplay.textFormat = Text.PlainText
        //         // textDisplay.text = fileContent
        //         // updateLineNumbers()
        //         // extractTimeRange()

        //         // 异步切换到富文本格式（不阻塞当前帧）
        //         Qt.callLater(function() {
        //             formattedFileContent = textAnalyzerPageRoot.formatForRichText(fileContent)
        //             textDisplay.textFormat = Text.RichText
        //             textDisplay.text = formattedFileContent
        //             updateLineNumbers()
        //             extractTimeRange()
        //         })
        //     } else {
        //         fileContent = ""
        //         formattedFileContent = ""
        //         textDisplay.textFormat = Text.PlainText
        //         textDisplay.text = "文件内容为空"
        //         startTime = ""
        //         endTime = ""
        //     }
        // }
    }

    // 更新行号的函数
    function updateLineNumbers() {
        var lineCount = textDisplay.lineCount
        var numbers = ""
        for (var i = 1; i <= lineCount; i++) {
            // 使用与文本显示区域相同的样式格式
            numbers += '<p style="margin: 0; padding: 4px 0; line-height: 1.5; border-bottom: 1px solid transparent; text-align: right;">' + i + '</p>'
        }
        lineNumberArea.text = numbers
    }

    // 提取时间范围的函数
    function extractTimeRange() {
        if (fileContent.length === 0) {
            startTime = ""
            endTime = ""
            return
        }

        var lines = fileContent.split('\n')
        var timeRegex = /(\d{2}\/\d{2}\/\d{2}\s+\d{2}:\d{2}:\d{2}\.\d{3})/

                // 提取开始时间（第一行）
                for (var i = 0; i < lines.length; i++) {
            var match = timeRegex.exec(lines[i])
            if (match) {
                startTime = match[1]
                break
            }
        }

        // 提取结束时间（最后一行，排除空行）
        for (var j = lines.length - 1; j >= 0; j--) {
            var line = lines[j].trim()
            if (line.length > 0) {
                var match = timeRegex.exec(line)
                if (match) {
                    endTime = match[1]
                    break
                }
            }
        }

        console.log("时间范围:", startTime, "至", endTime)
    }

    // 加载选中的文件
    function loadSelectedFile(filePath) {
        if (isLoadingFile) {
            console.log("正在加载文件，忽略新请求")
            return
        }

        console.log("加载选中文件:", filePath)
        isLoadingFile = true
        loadingIndicator.visible = true
        loadingIndicator.value = 0

        // 清理当前搜索状态
        searchInput.text = ""
        resultsModel.clear()
        searchResultsReady = false
        cachedSearchResults = []
        lastSearchText = ""
        cachedHighlightedContent = ""

        // 请求文件内容
        // fileHandler.requestFileContent(filePath) // 保留此行
        sqliteTextHandler.requestFileContent(filePath)
    }

    // 获取类别颜色
    function getCategoryColor(category) {
        switch (category) {
        case "主控文件": return "#DC2626"
        case "底盘文件": return "#EA580C"
        case "引导文件": return "#D97706"
        case "SC2000A文件": return "#059669"
        case "车辆文件": return "#0284C7"
        case "通用文本文件": return "#7C3AED"
        case "日志文件": return "#BE123C"
        default: return "#64748B"
        }
    }

    // 格式化文件大小
    function formatFileSize(bytes) {
        if (bytes === 0) return "0 B"

        var k = 1024
        var sizes = ["B", "KB", "MB", "GB"]
        var i = Math.floor(Math.log(bytes) / Math.log(k))

        return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i]
    }
}

