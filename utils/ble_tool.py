#!/usr/bin/env python3
"""
BLE Scanner and Explorer Tool
Scan for BLE devices and explore their services/characteristics
"""

import asyncio
import argparse
from bleak import BleakScanner, BleakClient


async def scan_devices(scan_time=5.0):
    """
    Scan for BLE devices and display them with RSSI values.
    
    Args:
        scan_time: Time in seconds to scan for devices
    """
    print(f"Scanning for BLE devices ({scan_time}s)...\n")
    
    # Use discover with return_adv=True to get AdvertisementData
    devices = await BleakScanner.discover(timeout=scan_time, return_adv=True)
    
    if not devices:
        print("No devices found.")
        return
    
    print(f"Found {len(devices)} device(s):\n")
    print(f"{'#':<4} {'Address':<20} {'RSSI':<6} {'Name':<30}")
    print("-" * 65)
    
    # Sort by RSSI from AdvertisementData
    sorted_devices = sorted(devices.items(), key=lambda item: item[1][1].rssi, reverse=True)
    
    for idx, (address, (device, adv_data)) in enumerate(sorted_devices, 1):
        name = adv_data.local_name if adv_data.local_name else "(Unknown)"
        rssi = adv_data.rssi
        print(f"{idx:<4} {address:<20} {rssi:<6} {name:<30}")


async def subscribe_characteristic(address, char_identifier, duration=30.0):
    """
    Subscribe to a BLE characteristic and monitor notifications/indications.
    
    Args:
        address: BLE device address (MAC address or UUID)
        char_identifier: Characteristic UUID or handle (e.g., "00002a37..." or "15")
        duration: How long to monitor in seconds (default: 30.0)
    """
    print(f"\nConnecting to {address}...\n")
    
    notification_count = 0
    
    def notification_handler(sender, data):
        nonlocal notification_count
        notification_count += 1
        timestamp = asyncio.get_event_loop().time()
        
        print(f"\n[{notification_count}] Notification received at {timestamp:.2f}s")
        print(f"  Sender: {sender}")
        print(f"  Data (hex): {data.hex()}")
        print(f"  Data (bytes): {data}")
        print(f"  Length: {len(data)} bytes")
        
        # Try to decode as string
        try:
            decoded = data.decode('utf-8')
            if decoded.isprintable():
                print(f"  Data (string): {decoded}")
        except:
            pass
        
        # Try to decode as integers
        if len(data) == 1:
            print(f"  Data (uint8): {int.from_bytes(data, 'little')}")
        elif len(data) == 2:
            print(f"  Data (uint16): {int.from_bytes(data, 'little')}")
            print(f"  Data (int16): {int.from_bytes(data, 'little', signed=True)}")
        elif len(data) == 4:
            print(f"  Data (uint32): {int.from_bytes(data, 'little')}")
            print(f"  Data (int32): {int.from_bytes(data, 'little', signed=True)}")
            print(f"  Data (float): {__import__('struct').unpack('f', data)[0]:.6f}")
    
    try:
        async with BleakClient(address) as client:
            print(f"Connected: {client.is_connected}")
            print(f"Address: {client.address}\n")
            
            # Try pairing if needed
            try:
                await client.pair()
            except Exception:
                pass
            
            # Determine if identifier is a handle (integer) or UUID
            char = None
            is_handle = False
            
            try:
                # Try to parse as integer handle
                handle = int(char_identifier)
                is_handle = True
                
                # Find characteristic by handle
                for service in client.services:
                    for c in service.characteristics:
                        if c.handle == handle:
                            char = c
                            break
                    if char:
                        break
                        
            except ValueError:
                # Not an integer, treat as UUID
                char_uuid_lower = char_identifier.lower()
                matching_chars = []
                
                # Find all characteristics with this UUID
                for service in client.services:
                    for c in service.characteristics:
                        if c.uuid.lower() == char_uuid_lower:
                            matching_chars.append(c)
                
                if len(matching_chars) == 1:
                    char = matching_chars[0]
                elif len(matching_chars) > 1:
                    print(f"Error: Multiple characteristics found with UUID {char_identifier}:")
                    print("\nPlease specify by handle instead:\n")
                    for c in matching_chars:
                        props = ', '.join(c.properties)
                        print(f"  Handle: {c.handle}")
                        print(f"    Service: {c.service_uuid}")
                        print(f"    Properties: {props}")
                        print()
                    print("Example usage:")
                    print(f"  python ble_tool.py subscribe {address} {matching_chars[0].handle}")
                    return
            
            if not char:
                identifier_type = "handle" if is_handle else "UUID"
                print(f"Error: Characteristic with {identifier_type} '{char_identifier}' not found!")
                print("\nAvailable characteristics with notify/indicate:")
                for service in client.services:
                    for c in service.characteristics:
                        if "notify" in c.properties or "indicate" in c.properties:
                            print(f"  UUID: {c.uuid}")
                            print(f"    Handle: {c.handle}")
                            print(f"    Description: {c.description}")
                            print(f"    Service: {service.uuid}")
                            print()
                return
            
            print(f"Characteristic found: {char.uuid}")
            print(f"Description: {char.description}")
            print(f"Handle: {char.handle}")
            print(f"Service: {char.service_uuid}")
            print(f"Properties: {', '.join(char.properties)}\n")
            
            # Check if characteristic supports notifications or indications
            if "notify" not in char.properties and "indicate" not in char.properties:
                print("Error: Characteristic does not support notifications or indications!")
                print(f"Available properties: {', '.join(char.properties)}")
                return
            
            # Subscribe to notifications
            print(f"Subscribing to notifications for {duration}s...")
            print("Press Ctrl+C to stop early\n")
            print("=" * 70)
            
            # Use handle for subscription to avoid ambiguity
            await client.start_notify(char.handle, notification_handler)
            
            try:
                await asyncio.sleep(duration)
            except KeyboardInterrupt:
                print("\n\nStopping (Ctrl+C detected)...")
            
            await client.stop_notify(char.handle)
            
            print("\n" + "=" * 70)
            print(f"\nSubscription ended.")
            print(f"Total notifications received: {notification_count}")
            
    except Exception as e:
        print(f"Error: {e}")


async def explore_device(address):
    """
    Connect to a BLE device and explore its services and characteristics.
    
    Args:
        address: BLE device address (MAC address or UUID)
    """
    print(f"\nConnecting to {address}...\n")
    
    try:
        async with BleakClient(address) as client:
            print(f"Connected: {client.is_connected}")
            print(f"Address: {client.address}")
            
            # Try to read device name from standard GAP characteristic
            device_name = "N/A"
            try:
                # Standard Device Name characteristic UUID
                DEVICE_NAME_UUID = "00002a00-0000-1000-8000-00805f9b34fb"
                name_bytes = await client.read_gatt_char(DEVICE_NAME_UUID)
                device_name = name_bytes.decode('utf-8')
            except Exception as e:
                # Device name characteristic not available or not readable
                if "0x0e" in str(e) or "Insufficient Authentication" in str(e):
                    device_name = "N/A (requires pairing)"
            
            print(f"Device Name: {device_name}")
            print(f"MTU Size: {client.mtu_size}")
            
            # Check if device is paired (Linux/BlueZ specific)
            try:
                is_paired = await client.pair()
                if is_paired:
                    print(f"Pairing: Successfully paired")
            except Exception as e:
                print(f"Pairing: Not required or already paired")
            
            print("\n" + "=" * 70)
            
            for service in client.services:
                print(f"\nService: {service.uuid}")
                print(f"  Description: {service.description}")
                
                for char in service.characteristics:
                    print(f"\n  Characteristic: {char.uuid}")
                    print(f"    Description: {char.description}")
                    print(f"    Properties: {', '.join(char.properties)}")
                    print(f"    Handle: {char.handle}")
                    
                    # Try to read if readable
                    if "read" in char.properties:
                        try:
                            value = await client.read_gatt_char(char.uuid)
                            print(f"    Value (hex): {value.hex()}")
                            print(f"    Value (bytes): {value}")
                            
                            # Try to decode as string if printable
                            try:
                                decoded = value.decode('utf-8')
                                if decoded.isprintable():
                                    print(f"    Value (string): {decoded}")
                            except:
                                pass
                                
                        except Exception as e:
                            error_msg = str(e)
                            if "0x0e" in error_msg or "Insufficient Authentication" in error_msg:
                                print(f"    Error: Requires pairing/authentication")
                            elif "0x02" in error_msg:
                                print(f"    Error: Read not permitted")
                            elif "0x05" in error_msg:
                                print(f"    Error: Insufficient authentication")
                            elif "0x06" in error_msg:
                                print(f"    Error: Request not supported")
                            else:
                                print(f"    Error reading: {error_msg}")
                    
                    # Show descriptors if any
                    if char.descriptors:
                        print(f"    Descriptors:")
                        for desc in char.descriptors:
                            print(f"      - {desc.uuid}: {desc.description}")
            
            print("\n" + "=" * 70)
            print("Exploration complete!")
            
    except Exception as e:
        print(f"Error: {e}")
        print("\nMake sure the device is in range and connectable.")


def main():
    parser = argparse.ArgumentParser(
        description="BLE Scanner and Explorer - Scan and explore Bluetooth Low Energy devices",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Scan for all BLE devices
  python ble_tool.py scan
  
  # Scan for 10 seconds
  python ble_tool.py scan -t 10
  
  # Explore a specific device
  python ble_tool.py explore AA:BB:CC:DD:EE:FF
  
  # Explore a device by UUID (macOS/iOS)
  python ble_tool.py explore 12345678-1234-1234-1234-123456789012
  
  # Subscribe to a characteristic (monitor notifications)
  python ble_tool.py subscribe AA:BB:CC:DD:EE:FF 00002a37-0000-1000-8000-00805f9b34fb
  
  # Subscribe using handle instead of UUID (when there are duplicates)
  python ble_tool.py subscribe AA:BB:CC:DD:EE:FF 15
  
  # Subscribe for 60 seconds
  python ble_tool.py subscribe AA:BB:CC:DD:EE:FF 15 -t 60
        """
    )
    
    subparsers = parser.add_subparsers(dest='mode', help='Operation mode')
    
    # Scan mode
    scan_parser = subparsers.add_parser('scan', help='Scan for BLE devices')
    scan_parser.add_argument(
        '-t', '--time',
        type=float,
        default=5.0,
        help='Scan time in seconds (default: 5.0)'
    )
    
    # Explore mode
    explore_parser = subparsers.add_parser('explore', help='Explore a BLE device')
    explore_parser.add_argument(
        'address',
        type=str,
        help='Device address (MAC address like AA:BB:CC:DD:EE:FF or UUID)'
    )
    
    # Subscribe mode
    subscribe_parser = subparsers.add_parser('subscribe', help='Subscribe to characteristic notifications')
    subscribe_parser.add_argument(
        'address',
        type=str,
        help='Device address (MAC address like AA:BB:CC:DD:EE:FF or UUID)'
    )
    subscribe_parser.add_argument(
        'characteristic',
        type=str,
        help='Characteristic UUID or handle (e.g., "00002a37-..." or "15")'
    )
    subscribe_parser.add_argument(
        '-t', '--time',
        type=float,
        default=30.0,
        help='Monitoring duration in seconds (default: 30.0)'
    )
    
    args = parser.parse_args()
    
    # Show help if no mode specified
    if not args.mode:
        parser.print_help()
        return
    
    # Run appropriate mode
    if args.mode == 'scan':
        asyncio.run(scan_devices(args.time))
    elif args.mode == 'explore':
        asyncio.run(explore_device(args.address))
    elif args.mode == 'subscribe':
        asyncio.run(subscribe_characteristic(args.address, args.characteristic, args.time))

if __name__ == "__main__":
    main()
