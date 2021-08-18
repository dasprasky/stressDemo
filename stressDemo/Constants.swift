//
//  Constants.swift
//  stressDemo
//
//  Created by Prasenjit on 16/8/2021.
//

import Foundation


struct constants {
    static let hrServiceUUID = "180D"
    static let hrCharacteristicUUID = "00002a37-0000-1000-8000-00805f9b34fb"
    
    static let tempServiceUUID = "00001809-0000-1000-8000-00805f9b34fb"
    static let tempCharacteristicUUID = "00002a1c-0000-1000-8000-00805f9b34fb"
}

enum BLEState:String {
    case connected
    case disconnected
    case connecting
}
enum ReservedFloatValues: UInt32 {
    case mder_POSITIVE_INFINITY = 0x007FFFFFE
    case mder_NaN = 0x007FFFFF
    case mder_NRes = 0x00800000
    case mder_RESERVED_VALUE = 0x00800001
    case mder_NEGATIVE_INFINITY = 0x00800002
}
enum ReservedSFloatValues: Int16 {
    case mder_S_POSITIVE_INFINITY = 0x07FE
    case mder_S_NaN = 0x07FF
    case mder_S_NRes = 0x0800
    case mder_S_RESERVED_VALUE = 0x0801
    case mder_S_NEGATIVE_INFINITY = 0x0802
}

let RESERVED_FLOAT_VALUES = [Double.infinity, Double.nan, Double.nan, Double.nan, -Double.infinity]

struct NORCharacteristicReader{
    // parts taken from nrf-toolbox
    
    static func readUInt8Value(ptr aPointer: inout UnsafeMutablePointer<UInt8>) -> UInt8 {
        let val = aPointer.pointee
        aPointer = aPointer.successor()
        return val
    }
    static func readFloatValue(ptr aPointer: inout UnsafeMutablePointer<UInt8>) -> Float32 {
        let tempData = CFSwapInt32LittleToHost(UnsafeMutablePointer<UInt32>(OpaquePointer (aPointer)).pointee)
        var mantissa = Int32(tempData & 0x00FFFFFF)
        let exponent = Int8(bitPattern: UInt8(tempData >> 24))
        var output: Float32 = 0
        
        if mantissa >= Int32(ReservedFloatValues.mder_POSITIVE_INFINITY.rawValue) &&
            mantissa <= Int32(ReservedFloatValues.mder_NEGATIVE_INFINITY.rawValue){
            output = Float32(RESERVED_FLOAT_VALUES[Int(mantissa - Int32(ReservedSFloatValues.mder_S_POSITIVE_INFINITY.rawValue))])
        }
        else{
            if mantissa >= 0x800000{
                mantissa = -((0xFFFFFF + 1) - mantissa)
            }
            let magnitude = pow(10.0, Double(exponent))
            output=Float32(mantissa) * Float32(magnitude)
        }
        aPointer = aPointer.advanced(by: 4)
        return output
    }
}
