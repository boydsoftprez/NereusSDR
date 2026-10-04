// NereusSDR for iOS: the PTT button test's Bluetooth button, through CoreBluetooth (debug copies only)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if PTT_PROBE
import Combine
import CoreBluetooth
import Foundation

/// What the Bluetooth side hands the probe.
enum PttProbeBluetoothEvent: Equatable, Sendable {
    /// Connecting, disconnecting, listening: a line for the log.
    case note(String)
    /// A value changed; `press` is whether the chosen rule counts it.
    case value(press: Bool, detail: String)
}

/// A Bluetooth PTT button for the button test (plan Task 65, question 2).
/// It looks for nearby Bluetooth devices, connects to the one chosen,
/// listens to every value the device offers to report, and hands each
/// change to the probe, which counts it as a press by the chosen rule.
/// A button that shows up only as a keyboard never reaches CoreBluetooth,
/// so it does not appear here.
@MainActor
final class PttProbeBluetooth: NSObject, ObservableObject {
    struct Device: Identifiable, Equatable {
        let id: UUID
        var name: String
        var signal: Int
    }

    @Published private(set) var status = "Not started"
    @Published private(set) var looking = false
    @Published private(set) var devices: [Device] = []
    @Published private(set) var connectedName: String?
    /// How many of the connected device's values report their changes.
    @Published private(set) var listening = 0
    @Published var rule: PttProbeBluetoothRule = .everyChange

    /// Each line for the log: a note, or a value change and whether the rule counts it as a press.
    var onEvent: (@MainActor (PttProbeBluetoothEvent) -> Void)?

    private var central: CBCentralManager?
    private var seen: [UUID: CBPeripheral] = [:]
    private var connected: CBPeripheral?
    private var lookWhenReady = false

    /// Starts looking. The first time, iOS asks to use Bluetooth.
    func look() {
        if central == nil {
            lookWhenReady = true
            central = CBCentralManager(delegate: self, queue: .main)
            return
        }
        scan()
    }

    func stopLooking() {
        central?.stopScan()
        looking = false
    }

    func connect(_ id: UUID) {
        guard let central, let peripheral = seen[id] else { return }
        stopLooking()
        disconnect()
        connected = peripheral
        peripheral.delegate = self
        status = "Connecting"
        central.connect(peripheral, options: nil)
    }

    func disconnect() {
        guard let central, let connected else { return }
        central.cancelPeripheralConnection(connected)
        self.connected = nil
        connectedName = nil
        listening = 0
    }

    private func scan() {
        guard let central, central.state == .poweredOn else { return }
        devices = []
        seen = [:]
        looking = true
        status = "Looking"
        central.scanForPeripherals(withServices: nil, options: nil)
    }

    static func hex(_ data: Data) -> String {
        data.isEmpty ? "empty" : data.map { String(format: "%02X", $0) }.joined(separator: " ")
    }

    static func stateText(_ state: CBManagerState) -> String {
        switch state {
        case .poweredOn: "Bluetooth on"
        case .poweredOff: "Bluetooth off"
        case .unauthorized: "Bluetooth not allowed for NereusSDR"
        case .unsupported: "No Bluetooth on this device"
        case .resetting: "Bluetooth restarting"
        default: "Bluetooth state unknown"
        }
    }
}

extension PttProbeBluetooth: @preconcurrency CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        status = Self.stateText(central.state)
        if central.state == .poweredOn, lookWhenReady {
            lookWhenReady = false
            scan()
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let advertised = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        guard let name = peripheral.name ?? advertised else { return }
        seen[peripheral.identifier] = peripheral
        let device = Device(id: peripheral.identifier, name: name, signal: RSSI.intValue)
        if let index = devices.firstIndex(where: { $0.id == device.id }) {
            devices[index] = device
        } else {
            devices.append(device)
        }
        devices.sort { $0.signal > $1.signal }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectedName = peripheral.name ?? "Unnamed device"
        status = "Connected"
        onEvent?(.note("connected to \(connectedName ?? "a device")"))
        peripheral.discoverServices(nil)
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: (any Error)?) {
        status = "Did not connect"
        connected = nil
        onEvent?(.note("did not connect: \(error?.localizedDescription ?? "no reason given")"))
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                        error: (any Error)?) {
        status = "Disconnected"
        if connected?.identifier == peripheral.identifier {
            connected = nil
            connectedName = nil
            listening = 0
        }
        onEvent?(.note("disconnected" + (error.map { ": \($0.localizedDescription)" } ?? "")))
    }
}

extension PttProbeBluetooth: @preconcurrency CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: (any Error)?) {
        for service in peripheral.services ?? [] {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService,
                    error: (any Error)?) {
        for characteristic in service.characteristics ?? []
        where characteristic.properties.contains(.notify) || characteristic.properties.contains(.indicate) {
            peripheral.setNotifyValue(true, for: characteristic)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: (any Error)?) {
        if characteristic.isNotifying {
            listening += 1
            onEvent?(.note("listening to value \(characteristic.uuid.uuidString)"))
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                    error: (any Error)?) {
        let value = characteristic.value ?? Data()
        onEvent?(.value(press: rule.isPress(value), detail: "value \(characteristic.uuid.uuidString) = \(Self.hex(value))"))
    }
}
#endif
