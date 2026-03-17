import QtQuick
import QtQuick3D

Window {
    id: root
    width: 800
    height: 480
    visible: true
    visibility: Window.FullScreen

    property string currentShape: "sensortag"
    property bool calibrating: false
    property int calibStep: 0
    property var calibTaps: []

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

    // ── MAIN VIEW ─────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        visible: !root.calibrating
        color: "#f0f0f0"

        View3D {
            x: 0; y: 0; width: 800; height: 370
            environment: SceneEnvironment {
                clearColor: "#dde8f5"
                backgroundMode: SceneEnvironment.Color
            }
            PerspectiveCamera { position: Qt.vector3d(0,0,300); clipNear:1; clipFar:10000 }
            DirectionalLight { eulerRotation: Qt.vector3d(-30,-30,0); brightness: 2.0 }
            DirectionalLight { eulerRotation: Qt.vector3d(30,60,0);   brightness: 1.0 }

            Node {
                visible: root.currentShape === "sensortag"
                // Stand the device upright: CAD has Z as long axis (51mm), Qt3D Y is up
                eulerRotation.x: -90
                // Spin around vertical axis
                NumberAnimation on eulerRotation.y { from:0; to:360; duration:8000; loops:Animation.Infinite; running:true }

                // Jacket: bbox center (23.9, 6.9, 25.5) mm, scale 4 → offset = -center*4
                Model {
                    source: "meshes/sensortag_jacket.mesh"
                    scale: Qt.vector3d(4,4,4)
                    position: Qt.vector3d(-95.6, -27.6, -102.0)
                    materials: PrincipledMaterial { baseColor: "#cc2200"; roughness: 0.6; metalness: 0.0 }
                }
                // Box: bbox center (18.3, 4.1, 25.5) mm, scale 4
                Model {
                    source: "meshes/sensortag_box.mesh"
                    scale: Qt.vector3d(4,4,4)
                    position: Qt.vector3d(-73.2, -16.4, -102.0)
                    materials: PrincipledMaterial { baseColor: "#ccccdd"; roughness: 0.4; metalness: 0.3 }
                }
            }
            Model {
                visible: root.currentShape === "sphere"
                source: "#Sphere"
                scale: Qt.vector3d(1.4,1.4,1.4)
                materials: PrincipledMaterial { baseColor: "#cc2222"; roughness: 0.15; metalness: 0.7 }
                NumberAnimation on eulerRotation.y { from:0; to:360; duration:5000; loops:Animation.Infinite; running:true }
                NumberAnimation on eulerRotation.x { from:0; to:360; duration:7000; loops:Animation.Infinite; running:true }
            }
        }

        Rectangle {
            x:0; y:370; width:800; height:110
            color: "#ffffff"
            Rectangle { x:0; y:0; width:800; height:2; color:"#aaaaaa" }

            Row {
                anchors.centerIn: parent
                spacing: 16

                Rectangle {
                    width:200; height:84; radius:10
                    color: root.currentShape==="sensortag" ? "#2266cc" : "#ccddff"
                    border.color: "#2266cc"; border.width: 3
                    Text {
                        anchors.centerIn: parent
                        text: "SENSOR TAG"; font.pixelSize: 20; font.bold: true
                        color: root.currentShape==="sensortag" ? "white" : "#2266cc"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: root.currentShape = "sensortag"
                    }
                }

                Rectangle {
                    width:200; height:84; radius:10
                    color: root.currentShape==="sphere" ? "#cc2222" : "#ffcccc"
                    border.color: "#cc2222"; border.width: 3
                    Text {
                        anchors.centerIn: parent
                        text: "3D BALL"; font.pixelSize: 22; font.bold: true
                        color: root.currentShape==="sphere" ? "white" : "#cc2222"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: root.currentShape = "sphere"
                    }
                }

                Rectangle {
                    width:200; height:84; radius:10
                    color: "#228833"; border.color: "#116622"; border.width: 3
                    Text {
                        anchors.centerIn: parent
                        text: "CALIBRATE"; font.pixelSize: 20; font.bold: true; color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.BlankCursor
                        onClicked: {
                            root.calibTaps = []
                            root.calibStep = 0
                            root.calibrating = true
                        }
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

        // Crosshair at current target position
        Item {
            id: crosshair
            visible: root.calibStep < 9
            property point tgt: root.calibStep < 9 ? root.targets[root.calibStep] : Qt.point(0,0)
            x: tgt.x - 40; y: tgt.y - 40
            width: 80; height: 80

            Rectangle { x:38; y:0;  width:4; height:80; color:"white" }
            Rectangle { x:0;  y:38; width:80; height:4;  color:"white" }
            Rectangle { x:30; y:30; width:20; height:20; color:"red"; radius:10 }

            // Large hit zone around crosshair
            MouseArea {
                x: -80; y: -80; width: 240; height: 240
                cursorShape: Qt.BlankCursor
                onClicked: {
                    // mouseX/Y relative to this area's origin
                    var tapX = mouseX + parent.x + 80
                    var tapY = mouseY + parent.y + 80
                    var taps = root.calibTaps
                    taps.push({
                        tx: tapX, ty: tapY,
                        gx: root.targets[root.calibStep].x,
                        gy: root.targets[root.calibStep].y
                    })
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

    // Least-squares linear fit then save
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
