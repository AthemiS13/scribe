import asyncio
from bleak import BleakClient, BleakScanner

# Replace with your ESP32's BLE name
TARGET_DEVICE_NAME = "ESP32_Bluetooth_Serial"
SERVICE_UUID = "12345678-1234-1234-1234-123456789012"  # Replace with your service UUID
CHARACTERISTIC_UUID = "87654321-4321-4321-4321-210987654321"  # Replace with your characteristic UUID

async def send_ble_message(message):
    # Discover devices
    print("Scanning for BLE devices...")
    devices = await BleakScanner.discover()

    esp_device = None
    for device in devices:
        if device.name and TARGET_DEVICE_NAME in device.name:  # Check for matching device name
            esp_device = device
            print(f"Found target device: {device.name} - {device.address}")
            break

    if not esp_device:
        print("Target device not found.")
        return

    # Connect and send message
    async with BleakClient(esp_device.address) as client:
        print("Connected to device.")

        # Get the services and find the characteristic
        services = await client.get_services()
        for service in services:
            if service.uuid == SERVICE_UUID:
                # Look for the characteristic in the service
                for char in service.characteristics:
                    if char.uuid == CHARACTERISTIC_UUID:
                        await client.write_gatt_char(char, message.encode())
                        print(f"Message sent: {message}")
                        return
        print("Characteristic not found in the specified service.")

# Replace 'Hello, ESP32!' with your message
asyncio.run(send_ble_message("I Love Scribe hello Found target device: ESP32_Bluetooth_Serial - 24:EC:4A:1F:3E:F1 !"))
