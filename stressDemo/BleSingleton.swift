//
//  BleSingleton.swift
//  stressDemo
//
//  Created by Prasenjit on 16/8/2021.
//

import UIKit
import Foundation
import CoreBluetooth

protocol BleSingletonDelegate: AnyObject {
    func hrDidChange(newVariableValue value: UInt8)
    func tempDidChange(newVariableValue value: Double)
    func tempInFarhrenheitDidChange(newVariableValue value: Bool)
    func respRateDidChange(newVariableValue value: UInt8)
    func curRMSSDDidChange(newVariableValue value: Float)
}

class BleSingleton: NSObject{
    var heart_rate: UInt8? {
        didSet{
            delegate?.hrDidChange(newVariableValue: heart_rate ?? 0)
        }
    }
    var final_temp: Double? {
        didSet{
            delegate?.tempDidChange(newVariableValue: final_temp ?? 0.0)
        }
    }
    var tempInFahrenheit: Bool?{
        didSet{
            delegate?.tempInFarhrenheitDidChange(newVariableValue: tempInFahrenheit ?? false)
        }
    }
    var respRate: UInt8? {
        didSet{
            delegate?.respRateDidChange(newVariableValue: respRate ?? 0)
        }
    }
    var curRMSSD: Float? {
        didSet{
            delegate?.curRMSSDDidChange(newVariableValue: curRMSSD ?? 0.0)
        }
    }
    
    
    var numDevicesFound: Int = 0
    
    weak var delegate: BleSingletonDelegate?
    
    // MARK: - BLE shared instance
    
    static let shared = BleSingleton()
    
    // MARK: - Properties
    
    //CoreBluetooth properties
    var centralManager:CBCentralManager!
    var activeDevice:CBPeripheral?
    var activeCharacteristic:CBCharacteristic?
    
    
    //Device UUID properties
    struct myDevice {
        static var ServiceUUID:CBUUID?
        static var CharactersticUUID:CBUUID?
    }
    
    // MARK: - Init method
    
    private override init() { }
    
    private struct Peripheral{
        let peripheral: CBPeripheral;
        var rssi: Int;
        var name: String;
    }
    private var peripherals: [Peripheral] = []
    private var rri_count:Int = 0


}

extension BleSingleton: CBCentralManagerDelegate{
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("Central is powered on")
            startScanningForDevicesWith()
            break
        case .poweredOff: self.clearDevices()
        case .resetting: self.disconnect()
        case .unauthorized: break
        case .unsupported: break
        case .unknown: break
        @unknown default:
            print("Central is not powered on")
            break
        }
    }
    
    func startScanningForDevicesWith() {
        self.disconnect()
        centralManager.scanForPeripherals(withServices: [CBUUID(string: constants.hrServiceUUID)], options: nil)
        }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
        advertisementData: [String : Any], rssi RSSI: NSNumber) {
//        print("\n")
        let d_name = peripheral.name ?? "Unknown"
//        print("New peripheral found: ", d_name, "rssi: ", RSSI.stringValue)
        if d_name.contains("Hera"){
            peripherals.append(Peripheral(peripheral: peripheral, rssi: RSSI.intValue, name: (peripheral.name ?? "Unknown")))
            numDevicesFound+=1
        }
        
        
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
            centralManager.stopScan()
            postBLEConnectionStateNotification(.connecting)
            activeDevice = peripheral
            activeDevice?.delegate = self
            activeDevice?.discoverServices([CBUUID(string: constants.hrServiceUUID), CBUUID(string: constants.tempServiceUUID)])
        }
        
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
            //create alert
        }
        
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        if peripheral == activeDevice {
                postBLEConnectionStateNotification(.disconnected)
                clearDevices()
            }
        }
}


extension BleSingleton: CBPeripheralDelegate{
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?){
        guard let services = peripheral.services else { return}
        for thisService in services {
//            print(thisService.uuid)
            if thisService.uuid == CBUUID(string: constants.hrServiceUUID) ||
                thisService.uuid == CBUUID(string: constants.tempServiceUUID){
                activeDevice?.discoverCharacteristics(nil, for: thisService)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if error != nil {
            // alret error
            // post notification
            return
        }
        guard let characteristics = service.characteristics else { return }
        postBLEConnectionStateNotification(.connected)
        for thisCharacteristic in characteristics {
            if thisCharacteristic.uuid == CBUUID(string: constants.hrCharacteristicUUID) ||
                thisCharacteristic.uuid == CBUUID(string: constants.tempCharacteristicUUID){
                activeCharacteristic = thisCharacteristic
                peripheral.setNotifyValue(true, for: activeCharacteristic!)
//                print("respiration init")
                init_respiration_analysis()
                init_hrv_analysis()
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if error != nil {
            print("Error occured while updating characteristic value: \(error!.localizedDescription)")
            return
        }
        
        guard let dataFromDevice = characteristic.value else { return}
        var array = UnsafeMutablePointer<UInt8> (OpaquePointer (((dataFromDevice as NSData?)?.bytes)!))
        
        if characteristic.uuid == CBUUID(string: constants.hrCharacteristicUUID) {
            postRecievedDataFromDeviceNotification()
//            print("hr: ", dataFromDevice.hexStringEncoded())
            let hrp_hex = dataFromDevice.hexStringEncoded()
            let heart_rate_str = hrp_hex[2..<4]
            heart_rate = UInt8(heart_rate_str, radix: 16)
//            print(heart_rate ?? "")
            
            if(hrp_hex.length > 4){
                for i in stride(from: 4, through: hrp_hex.length - 4, by: 4) {
                    rri_count+=1
                    let rri = UInt16( hrp_hex[i+2] + hrp_hex[i+3]
                                      + hrp_hex[i] + hrp_hex[i+1], radix:16)
//                    print("rri: ", rri!)
                    
                    hrv_analysis(Int32(NSDate().timeIntervalSince1970), heart_rate!, Int16(rri!))
                    analyze_respiration(Float(rri!))
                }
            }
        }
        
        else if characteristic.uuid == CBUUID(string: constants.tempCharacteristicUUID){

            let flags = NORCharacteristicReader.readUInt8Value(ptr: &array)
              tempInFahrenheit = (flags & 0x01) > 0
//            let timestampPresent: Bool = (flags & 0x02) > 0
//            let typePresent: Bool = (flags & 0x04) > 0
            
            let tempValue: Float = NORCharacteristicReader.readFloatValue(ptr: &array)
//            if tempInFahrenheit == false{
//                print(tempValue)
//                let tempValueinFar = tempValue * 9.0 / 5.0 + 32.0
//            }
//            if tempInFahrenheit == true{
//                let tempValueinCel = (tempValue - 32.0) * 5.0 / 9.0
//            }
            final_temp = Double(tempValue) // observe this

        }
        if rri_count > 30 {
            let resp: respiration_result_t = get_respiration_result()
            respRate = resp.respiratory_rate
//            print("Respiratory rate: ",  respRate!)
            
            let hrv: hrv_result_t = HRV_Get_Analysis_Result()
            
//            let percentrage_stress: Int = get_percentage_stress(hrv.getRMSSD(), age)
            curRMSSD = hrv.rMSSD
        }
    }
}


extension BleSingleton{
    // MARK: BLE Methods
    
    public func startCentralManager() {
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
//    public func startScanning() {
//        startScanningForDevicesWith()
//    }
    
    public func completeScanning(){
        self.centralManager.stopScan()
//        print("StopScan")
        guard !self.peripherals.isEmpty else {
            return
        }
        self.peripherals.sort(by: { $0.rssi > $1.rssi})
        //if peripherals != 0
//        print(self.peripherals[0].rssi)
        self.centralManager.connect(self.peripherals[0].peripheral,
            options: [CBConnectPeripheralOptionNotifyOnNotificationKey : true])
    }
    
    func resetCentralManger() {
        self.disconnect()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    public func disconnect() {
        if let activeCharacteristic = activeCharacteristic {
            activeDevice?.setNotifyValue(false, for: activeCharacteristic)
        }
        if let activeDevice = activeDevice {
            centralManager.cancelPeripheralConnection(activeDevice)
        }
    }
    
    fileprivate func clearDevices() {
        activeDevice = nil
        activeCharacteristic = nil
        myDevice.ServiceUUID = nil
        myDevice.CharactersticUUID = nil
    }
    
//        fileprivate func createErrorAlert() {
//        deviceAlert = UIAlertController(title: "Error: failed to connect.",
//            message: "Please try again.", preferredStyle: .alert)
//    }
    
    // MARK: NSNotificationCenter Methods
    
    fileprivate func postBLEConnectionStateNotification(_ state: BLEState) {
        let connectionDetails = ["currentState" : state]
        NotificationCenter.default.post(name: .BLE_State_Notification, object: self, userInfo: connectionDetails)
    }
    
    fileprivate func postRecievedDataFromDeviceNotification() {
        NotificationCenter.default.post(name: .BLE_Data_Notification, object: self, userInfo: nil)
    }
    func decodeHRValue(withData data: Data) -> Int {

         let count = data.count / MemoryLayout<UInt8>.size

         var array = [UInt8](repeating: 0, count: count)

         (data as NSData).getBytes(&array, length:count * MemoryLayout<UInt8>.size)

         

         var bpmValue : Int = 0;

         if ((array[0] & 0x01) == 0) {

             bpmValue = Int(array[1])

         } else {

             //Convert Endianess from Little to Big

             bpmValue = Int(UInt16(array[2] * 0xFF) + UInt16(array[1]))

         }

         return bpmValue

     }
    
}
public extension Data {
    private static let hexAlphabet = Array("0123456789abcdef".unicodeScalars)
    func hexStringEncoded() -> String {
        String(reduce(into: "".unicodeScalars) { result, value in
            result.append(Self.hexAlphabet[Int(value / 0x10)])
            result.append(Self.hexAlphabet[Int(value % 0x10)])
        })
    }
}
extension String {

    var length: Int {
        return count
    }

    subscript (i: Int) -> String {
        return self[i ..< i + 1]
    }

    func substring(fromIndex: Int) -> String {
        return self[min(fromIndex, length) ..< length]
    }

    func substring(toIndex: Int) -> String {
        return self[0 ..< max(0, toIndex)]
    }

    subscript (r: Range<Int>) -> String {
        let range = Range(uncheckedBounds: (lower: max(0, min(length, r.lowerBound)),
                                            upper: min(length, max(0, r.upperBound))))
        let start = index(startIndex, offsetBy: range.lowerBound)
        let end = index(start, offsetBy: range.upperBound - range.lowerBound)
        return String(self[start ..< end])
    }
}
