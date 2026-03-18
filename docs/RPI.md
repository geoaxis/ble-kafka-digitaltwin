# Raspberry Pi Setup

Fresh Raspbian Bookworm to working kiosk in 8 steps.

## 1. Packages

```bash
sudo apt update && sudo apt install -y \
  qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools \
  qt6-quick3d-dev qt6-quick3d-dev-tools qt6-connectivity-dev qt6-shadertools-dev \
  qt6-qpa-plugins qt6-shader-baker \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtquick3d qml6-module-qtquick3d-helpers \
  qml6-module-qtcore qml6-module-qtqml qml6-module-qtqml-models \
  librdkafka-dev \
  bluez libdbus-1-dev tslib libts-dev
```

## 2. Display + touchscreen

Append to `/boot/firmware/config.txt`:

```ini
[all]
hdmi_force_hotplug=1
dtparam=i2c_arm=on
dtparam=spi=on
display_rotate=0
max_usb_current=1
config_hdmi_boost=7
hdmi_group=2
hdmi_mode=87
hdmi_drive=1
hdmi_cvt 800 480 60 6 0 0 0
dtoverlay=ads7846,cs=1,penirq=25,penirq_pull=2,speed=50000,keep_vref_on=0,swapxy=0,pmax=255,xohms=150,xmin=200,xmax=3900,ymin=200,ymax=3900
dtoverlay=vc4-kms-v3d
gpu_mem=128
```

## 3. Bluetooth

```bash
sudo systemctl enable bluetooth
```

## 4. D-Bus permissions

Create `/etc/dbus-1/system.d/kiosk-ble.conf`:

```xml
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy user="hatim">
    <allow send_destination="org.bluez"/>
    <allow send_interface="org.freedesktop.DBus.ObjectManager"/>
    <allow send_interface="org.freedesktop.DBus.Properties"/>
    <allow send_interface="org.bluez.GattCharacteristic1"/>
    <allow send_interface="org.bluez.Device1"/>
    <allow send_interface="org.bluez.Adapter1"/>
  </policy>
</busconfig>
```

## 5. Kiosk environment

Create `/etc/kiosk.env`:

```env
QT_QPA_PLATFORM=eglfs
QT_QPA_EGLFS_INTEGRATION=eglfs_kms
QT_QPA_EGLFS_KMS_CONFIG=/etc/kiosk-eglfs.json
QT_QPA_EGLFS_WIDTH=800
QT_QPA_EGLFS_HEIGHT=480
QT_QPA_EGLFS_DEPTH=16
QT_QPA_EGLFS_ALWAYS_SET_MODE=1
QT_QPA_EGLFS_HIDECURSOR=1
QT_QPA_EGLFS_DISABLE_INPUT=0
QT_QPA_GENERIC_PLUGINS=tslib:/dev/input/event2
TSLIB_TSDEVICE=/dev/input/event2
TSLIB_CALIBFILE=/etc/pointercal
TSLIB_CONFFILE=/etc/ts.conf
TSLIB_FBDEVICE=/dev/fb0
QT_LOGGING_RULES=*.debug=false
KAFKA_BROKERS=your-broker.confluent.cloud:9092
KAFKA_TOPIC=sensortag-imu
KAFKA_API_KEY=your-api-key
KAFKA_API_SECRET=your-api-secret
```

Create `/etc/kiosk-eglfs.json`:

```json
{"device": "/dev/dri/card0", "outputs": [{"name": "HDMI1"}]}
```

## 6. Systemd service

Create `/etc/systemd/system/kiosk.service`:

```ini
[Unit]
Description=Qt Kiosk
After=local-fs.target dev-dri-card0.device bluetooth.service
Wants=dev-dri-card0.device bluetooth.service
DefaultDependencies=no

[Service]
Type=simple
User=hatim
EnvironmentFile=/etc/kiosk.env
ExecStart=/usr/local/bin/kiosk
Restart=always
RestartSec=1
StandardOutput=null
StandardError=journal

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload && sudo systemctl enable kiosk
```

## 7. Build + deploy

```bash
cd ~/kiosk/src && qmake6 kiosk.pro && make -j4
sudo cp kiosk /usr/local/bin/kiosk && sudo systemctl start kiosk
```

## 8. Boot optimization (optional, gets to ~12s)

```bash
sudo systemctl disable cloud-init cloud-init-local cloud-config cloud-final \
  ModemManager NetworkManager-wait-online serial-getty@ttyS0
sudo systemctl mask fstrim.timer
```

Append to `/boot/firmware/config.txt`:

```ini
boot_delay=0
disable_splash=1
avoid_warnings=1
```

Set `/boot/firmware/cmdline.txt` to:

```
console=tty1 root=PARTUUID=<yours> rootfstype=ext4 fsck.repair=yes rootwait quiet loglevel=3 noresume elevator=noop rd.systemd.show_status=false rd.udev.log_level=3
```
