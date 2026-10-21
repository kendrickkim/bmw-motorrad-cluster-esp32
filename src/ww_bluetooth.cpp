#include "ww_bluetooth.h"
#include "car_status.h"

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#endif

WW_BLUETOOTH wwBluetooth;

// NimBLE only delivers BLECharacteristic::notify() to CCCD-subscribed peers.
// Older Bluedroid code forced CCCD via BLE2902; that descriptor is unavailable
// on NimBLE, so send notifications directly to connected peers.
static void forceNotifyCharacteristic(BLECharacteristic *chr, BLEServer *server)
{
    if (chr == NULL || server == NULL)
    {
        return;
    }

#if defined(CONFIG_NIMBLE_ENABLED)
    String value = chr->getValue();
    if (value.length() == 0)
    {
        return;
    }

    auto peers = server->getPeerDevices(false);
    for (auto const &peer : peers)
    {
        uint16_t conn = peer.first;
        if (server->getPeerMTU(conn) == 0)
        {
            continue;
        }

        os_mbuf *om = ble_hs_mbuf_from_flat((uint8_t *)value.c_str(), value.length());
        if (om == NULL)
        {
            continue;
        }

        int rc = ble_gatts_notify_custom(conn, chr->getHandle(), om);
        if (rc != 0)
        {
            os_mbuf_free_chain(om);
        }
    }
#else
    chr->notify();
#endif
}

void WW_BLUETOOTH_SERVER_CALLBACK::onConnect(BLEServer *pServer)
{
    D_INFO("BLE central connected\n");
    // Keep PROPERTY_NOTIFY enabled (NimBLE BLE2902 CCCD object is not usable).
    if (input != NULL)
    {
        input->setNotifyProperty(true);
    }
    if (wwBluetooth.pCharacteristic_rwni != NULL)
    {
        wwBluetooth.pCharacteristic_rwni->setNotifyProperty(true);
    }

    is_connected = true;
}

void WW_BLUETOOTH_SERVER_CALLBACK::onDisconnect(BLEServer *pServer)
{
    D_INFO("BLE central disconnected\n");
    pServer->getAdvertising()->start();

    is_connected = false;
}

void WW_BLUETOOTH_CHAR_CALLBACK::onWrite(BLECharacteristic *pCharacteristic)
{
    D_INFO("BLE characteristic write: %s\n", pCharacteristic->getUUID().toString().c_str());
}

void WW_BLUETOOTH_CHAR_CALLBACK::onRead(BLECharacteristic *pCharacteristic)
{
    D_INFO("BLE characteristic read: %s\n", pCharacteristic->getUUID().toString().c_str());
    if (pCharacteristic->getUUID().equals(BLEUUID(BLE_CHARACTERISTIC_CARSTATUS)))
    {
    }
    else
    {
        D_ERROR("Unknown characteristic read: %s\n", pCharacteristic->getUUID().toString().c_str());
    }
}

void WW_BLUETOOTH::CarStatusNotify(String value)
{
    if (pServer->getConnectedCount() > 0 && pServerCallback.is_connected && pCharacteristic_rwni != NULL)
    {
        pCharacteristic_rwni->setValue(value.c_str());
        forceNotifyCharacteristic(pCharacteristic_rwni, pServer);
    }
}

void WW_BLUETOOTH::CarStatusNotify(uint8_t *data, size_t size)
{
    if (pServer->getConnectedCount() > 0 && pServerCallback.is_connected && pCharacteristic_rwni != NULL)
    {
        pCharacteristic_rwni->setValue(data, size);
        forceNotifyCharacteristic(pCharacteristic_rwni, pServer);
    }
}

void WW_BLUETOOTH::KeyboardInputNotify(uint8_t *data, size_t size)
{
    if (pServer->getConnectedCount() > 0 && pServerCallback.is_connected && pServerCallback.input != NULL)
    {
        pServerCallback.input->setValue(data, size);
        forceNotifyCharacteristic(pServerCallback.input, pServer);
    }
}

static const uint8_t REPORT_MAP[] = {
    USAGE_PAGE(1), 0x01,      // Generic Desktop Controls
    USAGE(1), 0x06,           // Keyboard
    COLLECTION(1), 0x01,      // Application
    REPORT_ID(1), 0x01,       //   Report ID (1)
    USAGE_PAGE(1), 0x07,      //   Keyboard/Keypad
    USAGE_MINIMUM(1), 0xE0,   //   Keyboard Left Control
    USAGE_MAXIMUM(1), 0xE7,   //   Keyboard Right Control
    LOGICAL_MINIMUM(1), 0x00, //   Each bit is either 0 or 1
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1), 0x08, //   8 bits for the modifier keys
    REPORT_SIZE(1), 0x01,
    HIDINPUT(1), 0x02,     //   Data, Var, Abs
    REPORT_COUNT(1), 0x01, //   1 byte (unused)
    REPORT_SIZE(1), 0x08,
    HIDINPUT(1), 0x01,     //   Const, Array, Abs
    REPORT_COUNT(1), 0x06, //   6 bytes (for up to 6 concurrently pressed keys)
    REPORT_SIZE(1), 0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65, //   101 keys
    USAGE_MINIMUM(1), 0x00,
    USAGE_MAXIMUM(1), 0x65,
    HIDINPUT(1), 0x00,     //   Data, Array, Abs
    REPORT_COUNT(1), 0x05, //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1), 0x01,
    USAGE_PAGE(1), 0x08,    //   LEDs
    USAGE_MINIMUM(1), 0x01, //   Num Lock
    USAGE_MAXIMUM(1), 0x05, //   Kana
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    HIDOUTPUT(1), 0x02,    //   Data, Var, Abs
    REPORT_COUNT(1), 0x01, //   3 bits (Padding)
    REPORT_SIZE(1), 0x03,
    HIDOUTPUT(1), 0x01, //   Const, Array, Abs
    END_COLLECTION(0)   // End application collection
};

void WW_BLUETOOTH::On()
{

    if (isOn == true)
        return;

    BLEDevice::init("test_ble");
    BLEAddress bt_addr = BLEDevice::getAddress();
    String bt_mac = bt_addr.toString().c_str();
    bt_mac.toUpperCase();
    char mac_bt[20];
    strcpy(mac_bt, bt_mac.c_str() + 9); // 뒷 8자리
    BLEDevice::deinit();

    BLEDevice::init(StringFormat("BMW-WWC-%s", mac_bt).c_str());
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(&pServerCallback);

    hid = new BLEHIDDevice(pServer);

    BLEService *pInfoService = pServer->getServiceByUUID(BLEUUID((uint16_t)0x180a)); // Device Information Service
    // pService = pServer->createService(BLE_SERVICE_UUID);
    pCharacteristic_rwni = pInfoService->createCharacteristic(
        (uint16_t)0x2A01,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic_rwni->setCallbacks(&pNotificationCharacteristicCallback);
    printf("pInfoService: %s\n", pInfoService->getUUID().toString().c_str());
    printf("pCharacteristic_rwni: %s\n", pCharacteristic_rwni->getUUID().toString().c_str());

    pServerCallback.input = hid->inputReport(1);
    pServerCallback.output = hid->outputReport(1);
    pServerCallback.output->setCallbacks(&pKeyboardCharacteristicCallback);

    hid->manufacturer()->setValue("BMW-WWC");
    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid->hidInfo(0x00, 0x02);

    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

    hid->reportMap((uint8_t *)REPORT_MAP, sizeof(REPORT_MAP));
    hid->startServices();

    hid->setBatteryLevel(100);

    // pService->start();
    BLEAdvertising *advertising = pServer->getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->addServiceUUID(hid->deviceInfo()->getUUID());
    advertising->addServiceUUID(hid->batteryService()->getUUID());

    advertising->start();

    isOn = true;
}

void WW_BLUETOOTH::Off()
{
    if (isOn == false)
        return;
    if (pServer != NULL)
        pServer->getAdvertising()->stop();
    if (pService != NULL)
        pService->stop();
    // delete pCharacteristic;
    pServer->removeService(pService);

    BLEDevice::deinit();
    btStop();
    isOn = false;
    // LedControl.leds[LEDCONTROL::BLUE].status = LED_STATUS::LED_OFF;
}

size_t WW_BLUETOOTH::keyPress(uint8_t k)
{
    uint8_t i;
    if (k >= 136)
    { // it's a non-printing key (not a modifier)
        k = k - 136;
    }
    else if (k >= 128)
    { // it's a modifier key
        _keyReport.modifiers |= (1 << (k - 128));
        k = 0;
    }
    else
    { // it's a printing key
        k = pgm_read_byte(_asciimap + k);
        if (!k)
        {
            return 0;
        }
        if (k & 0x80)
        {                                 // it's a capital letter or other character reached with shift
            _keyReport.modifiers |= 0x02; // the left shift modifier
            k &= 0x7F;
        }
    }

    // Add k to the key report only if it's not already present
    // and if there is an empty slot.
    if (_keyReport.keys[0] != k && _keyReport.keys[1] != k &&
        _keyReport.keys[2] != k && _keyReport.keys[3] != k &&
        _keyReport.keys[4] != k && _keyReport.keys[5] != k)
    {

        for (i = 0; i < 6; i++)
        {
            if (_keyReport.keys[i] == 0x00)
            {
                _keyReport.keys[i] = k;
                break;
            }
        }
        if (i == 6)
        {
            return 0;
        }
    }
    wwBluetooth.KeyboardInputNotify((uint8_t *)&_keyReport, sizeof(KeyReport));
    return 1;
}

size_t WW_BLUETOOTH::keyRelease(uint8_t k)
{
    uint8_t i;
    if (k >= 136)
    { // it's a non-printing key (not a modifier)
        k = k - 136;
    }
    else if (k >= 128)
    { // it's a modifier key
        _keyReport.modifiers &= ~(1 << (k - 128));
        k = 0;
    }
    else
    { // it's a printing key
        k = pgm_read_byte(_asciimap + k);
        if (!k)
        {
            return 0;
        }
        if (k & 0x80)
        {                                    // it's a capital letter or other character reached with shift
            _keyReport.modifiers &= ~(0x02); // the left shift modifier
            k &= 0x7F;
        }
    }

    // Test the key report to see if k is present.  Clear it if it exists.
    // Check all positions in case the key is present more than once (which it shouldn't be)
    for (i = 0; i < 6; i++)
    {
        if (0 != k && _keyReport.keys[i] == k)
        {
            _keyReport.keys[i] = 0x00;
        }
    }

    wwBluetooth.KeyboardInputNotify((uint8_t *)&_keyReport, sizeof(KeyReport));
    return 1;
}