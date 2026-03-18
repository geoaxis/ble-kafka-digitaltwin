import QtQuick
import QtQuick3D

Window {
    id: root
    width: 800
    height: 480
    visible: true
    visibility: Window.FullScreen

    // screens: "welcome" | "scanning" | "connecting" | "connected"
    property string screen: "welcome"
    property string connectingName: ""
    property bool calibrating: false
    property int calibStep: 0
    property var calibTaps: []

    readonly property int modelRX: 90
    readonly property int modelRY: 0
    readonly property int modelRZ: 180

    readonly property var targets: [
        Qt.point(160,  60), Qt.point(400,  60), Qt.point(640,  60),
        Qt.point(160, 240), Qt.point(400, 240), Qt.point(640, 240),
        Qt.point(160, 420), Qt.point(400, 420), Qt.point(640, 420)
    ]
    readonly property var cornerLabels: [
        "Top-Left","Top-Center","Top-Right",
        "Mid-Left","Center","Mid-Right",
        "Bottom-Left","Bottom-Center","Bottom-Right"
    ]

    Connections {
        target: sensorTag
        function onConnectedChanged() {
            if (sensorTag.connected) {
                root.screen = "connected"
            } else if (root.screen === "connected" || root.screen === "connecting") {
                root.screen = "welcome"
            }
        }
    }

    // ── WELCOME SCREEN ────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        visible: !root.calibrating && root.screen === "welcome"
        color: "#1a1a2e"

        View3D {
            x: 0; y: 0; width: 800; height: 370
            environment: SceneEnvironment {
                clearColor: "#1a1a2e"
                backgroundMode: SceneEnvironment.Color
            }
            PerspectiveCamera { position: Qt.vector3d(0,0,300); clipNear:1; clipFar:10000 }
            DirectionalLight { eulerRotation: Qt.vector3d(-30,-30,0); brightness: 2.0 }
            DirectionalLight { eulerRotation: Qt.vector3d(30,60,0);   brightness: 1.0 }

            Node {
                eulerRotation.x: root.modelRX
                eulerRotation.y: root.modelRY
                eulerRotation.z: root.modelRZ

                Model {
                    source: "meshes/sensortag_jacket.mesh"
                    scale: Qt.vector3d(4,4,4)
                    position: Qt.vector3d(-95.6, -27.6, -102.0)
                    materials: PrincipledMaterial { baseColor: "#cc2200"; roughness: 0.6; metalness: 0.0 }
                }
                Model {
                    source: "meshes/sensortag_box.mesh"
                    scale: Qt.vector3d(4,4,4)
                    position: Qt.vector3d(-73.2, -16.4, -102.0)
                    materials: PrincipledMaterial { baseColor: "#ccccdd"; roughness: 0.4; metalness: 0.3 }
                }
            }
        }

        // Bottom bar: CONNECT + CALIBRATE
        Rectangle {
            x: 0; y: 370; width: 800; height: 110
            color: "#16213e"
            Rectangle { x:0; y:0; width:800; height:2; color:"#0f3460" }

            Row {
                anchors.centerIn: parent
                spacing: 20

                Rectangle {
                    width: 310; height: 80; radius: 12
                    color: "#0f3460"; border.color: "#4fc3f7"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "CONNECT TO SENSORTAG"
                        font.pixelSize: 20; font.bold: true; color: "#4fc3f7"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: { sensorTag.startScan(); root.screen = "scanning" }
                    }
                }

                Rectangle {
                    width: 190; height: 80; radius: 12
                    color: "#1a3a1a"; border.color: "#44cc44"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "CALIBRATE"
                        font.pixelSize: 20; font.bold: true; color: "#66ff66"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: { root.calibTaps = []; root.calibStep = 0; root.calibrating = true }
                    }
                }
            }
        }
    }

    // ── SCANNING SCREEN ───────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        visible: !root.calibrating && root.screen === "scanning"
        color: "#1a1a2e"

        // Header
        Text {
            x: 50; y: 28; width: 700
            horizontalAlignment: Text.AlignHCenter
            text: sensorTag.scanning ? "Searching for SensorTag..." : "Scan Complete"
            font.pixelSize: 26; font.bold: true
            color: sensorTag.scanning ? "#4fc3f7" : (sensorTag.devices.length > 0 ? "#66ff66" : "#ff8866")
        }
        Text {
            x: 50; y: 70; width: 700
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: sensorTag.scanning
                  ? "Scanning active — please wait"
                  : (sensorTag.devices.length > 0
                     ? "Tap a device below to connect"
                     : "No SensorTag found — wake it with the green button and scan again")
            font.pixelSize: 15; color: "#aaaacc"
        }

        // Device list
        ListView {
            visible: sensorTag.devices.length > 0
            x: 60; y: 110; width: 680; height: 250
            model: sensorTag.devices
            clip: true
            spacing: 6

            delegate: Rectangle {
                width: ListView.view.width
                height: 72
                color: devArea.pressed ? "#0f3460" : "#16213e"
                radius: 8
                border.color: "#4fc3f7"; border.width: 1

                Column {
                    anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
                    Text { text: modelData.name;    font.pixelSize: 20; font.bold: true; color: "white" }
                    Text { text: modelData.address; font.pixelSize: 14; color: "#aaaacc" }
                }
                Text {
                    anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                    text: "CONNECT ▶"
                    font.pixelSize: 16; font.bold: true; color: "#4fc3f7"
                }

                MouseArea {
                    id: devArea
                    anchors.fill: parent
                    cursorShape: Qt.BlankCursor
                    onClicked: {
                        root.connectingName = modelData.name
                        root.screen = "connecting"
                        sensorTag.connectToDevice(modelData.address)
                    }
                }
            }
        }

        // Bottom bar — absolute position, same pattern as welcome/connected screens
        Rectangle {
            x: 0; y: 370; width: 800; height: 110
            color: "#16213e"
            Rectangle { x:0; y:0; width:800; height:2; color:"#0f3460" }

            Row {
                anchors.centerIn: parent
                spacing: 20

                Rectangle {
                    width: 240; height: 76; radius: 12
                    color: sensorTag.scanning ? "#3a2a1a" : "#0f3460"
                    border.color: sensorTag.scanning ? "#cc8844" : "#4fc3f7"
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: sensorTag.scanning ? "STOP SCAN" : "SCAN AGAIN"
                        font.pixelSize: 20; font.bold: true
                        color: sensorTag.scanning ? "#ffaa44" : "#4fc3f7"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: {
                            if (sensorTag.scanning)
                                sensorTag.stopScan()
                            else
                                sensorTag.startScan()
                        }
                    }
                }

                Rectangle {
                    width: 240; height: 76; radius: 12
                    color: "#3a1a1a"; border.color: "#cc4444"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "CANCEL"
                        font.pixelSize: 20; font.bold: true; color: "#ff6666"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: { sensorTag.stopScan(); root.screen = "welcome" }
                    }
                }
            }
        }
    }

    // ── CONNECTING SCREEN ─────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        visible: !root.calibrating && root.screen === "connecting"
        color: "#1a1a2e"

        Column {
            anchors.centerIn: parent
            spacing: 22

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Connecting to"
                font.pixelSize: 18; color: "#aaaacc"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.connectingName
                font.pixelSize: 28; font.bold: true; color: "#4fc3f7"
            }

            // Animated progress dots
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16
                Repeater {
                    model: 3
                    Rectangle {
                        width: 14; height: 14; radius: 7
                        color: "#4fc3f7"
                        opacity: 0.2
                        SequentialAnimation on opacity {
                            running: root.screen === "connecting"
                            loops: Animation.Infinite
                            PauseAnimation   { duration: index * 350 }
                            NumberAnimation  { to: 1.0; duration: 350; easing.type: Easing.OutQuad }
                            NumberAnimation  { to: 0.2; duration: 350; easing.type: Easing.InQuad  }
                            PauseAnimation   { duration: (2 - index) * 350 }
                        }
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: sensorTag.status
                font.pixelSize: 15; color: "#7799bb"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            anchors { bottom: parent.bottom; bottomMargin: 20; horizontalCenter: parent.horizontalCenter }
            width: 160; height: 60; radius: 10
            color: "#3a1a1a"; border.color: "#cc4444"; border.width: 2
            Text {
                anchors.centerIn: parent
                text: "CANCEL"
                font.pixelSize: 18; font.bold: true; color: "#ff6666"
            }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.BlankCursor
                onClicked: { sensorTag.disconnectDevice(); root.screen = "welcome" }
            }
        }
    }

    // ── CONNECTED SCREEN ──────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        visible: !root.calibrating && root.screen === "connected"
        color: "#1a1a2e"

        View3D {
            x: 0; y: 0; width: 800; height: 370
            environment: SceneEnvironment {
                clearColor: "#1a1a2e"
                backgroundMode: SceneEnvironment.Color
            }
            PerspectiveCamera { position: Qt.vector3d(0,0,300); clipNear:1; clipFar:10000 }
            DirectionalLight { eulerRotation: Qt.vector3d(-30,-30,0); brightness: 2.0 }
            DirectionalLight { eulerRotation: Qt.vector3d(30,60,0);   brightness: 1.0 }

            Node {
                eulerRotation.x: 90
                eulerRotation.z: 180
                Node {
                    eulerRotation.x: -sensorTag.pitch
                    eulerRotation.y: -sensorTag.roll
                    eulerRotation.z: -sensorTag.yaw
                    Behavior on eulerRotation.x { NumberAnimation { duration: 80 } }
                    Behavior on eulerRotation.y { NumberAnimation { duration: 80 } }
                    Behavior on eulerRotation.z { NumberAnimation { duration: 80 } }

                    Model {
                        source: "meshes/sensortag_jacket.mesh"
                        scale: Qt.vector3d(4,4,4)
                        position: Qt.vector3d(-95.6, -27.6, -102.0)
                        materials: PrincipledMaterial { baseColor: "#cc2200"; roughness: 0.6; metalness: 0.0 }
                    }
                    Model {
                        source: "meshes/sensortag_box.mesh"
                        scale: Qt.vector3d(4,4,4)
                        position: Qt.vector3d(-73.2, -16.4, -102.0)
                        materials: PrincipledMaterial { baseColor: "#ccccdd"; roughness: 0.4; metalness: 0.3 }
                    }
                }
            }
        }

        // Debug overlay: raw sensor values + axis legend
        Rectangle {
            x: 4; y: 4
            width: 440; height: 110
            color: "#aa000000"; radius: 6

            Column {
                anchors { left: parent.left; leftMargin: 8; top: parent.top; topMargin: 6 }
                spacing: 2

                Text {
                    text: "Gyro  X: %1  Y: %2  Z: %3 °/s"
                        .arg(sensorTag.rawGx.toFixed(1))
                        .arg(sensorTag.rawGy.toFixed(1))
                        .arg(sensorTag.rawGz.toFixed(1))
                    font.pixelSize: 13; font.family: "Monospace"; color: "#ffffff"
                }
                Text {
                    text: "Accel X: %1  Y: %2  Z: %3 g"
                        .arg(sensorTag.rawAx.toFixed(2))
                        .arg(sensorTag.rawAy.toFixed(2))
                        .arg(sensorTag.rawAz.toFixed(2))
                    font.pixelSize: 13; font.family: "Monospace"; color: "#ffffff"
                }
                Text {
                    text: "P(x): %1°   Y(y): %2°   R(z): %3°"
                        .arg(sensorTag.pitch.toFixed(1))
                        .arg(sensorTag.yaw.toFixed(1))
                        .arg(sensorTag.roll.toFixed(1))
                    font.pixelSize: 13; font.family: "Monospace"; color: "#ffff88"
                }
                Row {
                    spacing: 14
                    Rectangle { width: 12; height: 12; radius: 2; color: "#ff3333" }
                    Text { text: "X axis"; font.pixelSize: 12; color: "#ff9999" }
                    Rectangle { width: 12; height: 12; radius: 2; color: "#33ff33" }
                    Text { text: "Y axis"; font.pixelSize: 12; color: "#99ff99" }
                    Rectangle { width: 12; height: 12; radius: 2; color: "#3333ff" }
                    Text { text: "Z axis"; font.pixelSize: 12; color: "#9999ff" }
                }
            }
        }

        // Bottom bar: DISCONNECT + ZERO (gyro bias recalibration)
        Rectangle {
            x:0; y:370; width:800; height:110
            color: "#16213e"
            Rectangle { x:0; y:0; width:800; height:2; color:"#0f3460" }

            // Calibrating indicator
            Text {
                anchors { top: parent.top; topMargin: 6; horizontalCenter: parent.horizontalCenter }
                visible: sensorTag.calibrating
                text: "Hold still — measuring gyro bias..."
                font.pixelSize: 14; color: "#ffcc44"
            }

            Row {
                anchors.centerIn: parent
                spacing: 24

                Rectangle {
                    width: 240; height: 76; radius: 10
                    color: "#3a1a1a"; border.color: "#cc4444"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "DISCONNECT"
                        font.pixelSize: 20; font.bold: true; color: "#ff6666"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: sensorTag.disconnectDevice()
                    }
                }

                Rectangle {
                    width: 200; height: 76; radius: 10
                    color: sensorTag.calibrating ? "#2a2a00" : "#1a1a3a"
                    border.color: sensorTag.calibrating ? "#888800" : "#4488ff"
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: sensorTag.calibrating ? "ZEROING..." : "ZERO"
                        font.pixelSize: 20; font.bold: true
                        color: sensorTag.calibrating ? "#888800" : "#4488ff"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        enabled: !sensorTag.calibrating
                        onClicked: sensorTag.zeroOrientation()
                    }
                }
            }
        }
    }

    // ── CALIBRATION VIEW ──────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        visible: root.calibrating
        color: "#111111"

        Text {
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 190 }
            visible: root.calibStep < 9
            text: "Tap the " + root.cornerLabels[root.calibStep] + " crosshair  (" + (root.calibStep+1) + "/9)"
            font.pixelSize: 26; color: "white"
            horizontalAlignment: Text.AlignHCenter
        }

        Item {
            id: crosshair
            visible: root.calibrating && root.calibStep < 9
            property point tgt: root.calibStep < 9 ? root.targets[root.calibStep] : Qt.point(0,0)
            x: tgt.x - 40; y: tgt.y - 40
            width: 80; height: 80

            Rectangle { x:38; y:0;  width:4; height:80; color:"white" }
            Rectangle { x:0;  y:38; width:80; height:4;  color:"white" }
            Rectangle { x:30; y:30; width:20; height:20; color:"red"; radius:10 }

            MouseArea {
                x: -80; y: -80; width: 240; height: 240
                cursorShape: Qt.BlankCursor
                onClicked: {
                    var tapX = mouseX + parent.x + 80
                    var tapY = mouseY + parent.y + 80
                    var taps = root.calibTaps
                    taps.push({ tx: tapX, ty: tapY, gx: root.targets[root.calibStep].x, gy: root.targets[root.calibStep].y })
                    root.calibTaps = taps
                    root.calibStep++
                    if (root.calibStep >= 9) saveTimer.start()
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: root.calibStep >= 9
            text: "Saving calibration...\nApp will restart."
            font.pixelSize: 28; color: "white"
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Timer {
        id: saveTimer
        interval: 600
        onTriggered: {
            var pts = root.calibTaps
            var n = pts.length
            var sumTx=0, sumGx=0, sumTxTx=0, sumTxGx=0
            var sumTy=0, sumGy=0, sumTyTy=0, sumTyGy=0
            for (var i = 0; i < n; i++) {
                sumTx   += pts[i].tx;  sumGx   += pts[i].gx
                sumTxTx += pts[i].tx * pts[i].tx
                sumTxGx += pts[i].tx * pts[i].gx
                sumTy   += pts[i].ty;  sumGy   += pts[i].gy
                sumTyTy += pts[i].ty * pts[i].ty
                sumTyGy += pts[i].ty * pts[i].gy
            }
            var mx = (n*sumTxGx - sumTx*sumGx) / (n*sumTxTx - sumTx*sumTx)
            var bx = (sumGx - mx*sumTx) / n
            var my = (n*sumTyGy - sumTy*sumGy) / (n*sumTyTy - sumTy*sumTy)
            var by = (sumGy - my*sumTy) / n
            calibrator.save(mx, bx, my, by)
        }
    }
}
