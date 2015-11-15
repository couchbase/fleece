//
//  Encoder.mm
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Encoder.hh"


namespace fleece {


    void encoder::write(__unsafe_unretained id obj) {
        if ([obj isKindOfClass: [NSString class]]) {
            nsstring_slice slice(obj);
            writeString(slice);
        } else if ([obj isKindOfClass: [NSNumber class]]) {
            switch ([obj objCType][0]) {
                case 'c':
                    // The only way to tell whether an NSNumber with 'char' type is a boolean is to
                    // compare it against the singleton kCFBoolean objects:
                    if (obj == (id)kCFBooleanTrue)
                        writeBool(true);
                    else if (obj == (id)kCFBooleanFalse)
                        writeBool(false);
                    else
                        writeInt([obj charValue]);
                    break;
                case 'f':
                    writeFloat([obj floatValue]);
                    break;
                case 'd':
                    writeDouble([obj doubleValue]);
                    break;
                case 'Q':
                    writeUInt([obj unsignedLongLongValue]);
                    break;
                default:
                    writeInt([obj longLongValue]);
                    break;
            }
        } else if ([obj isKindOfClass: [NSDictionary class]]) {
            encoder dict = writeDict((uint32_t)[obj count]);
            for (NSString* key in obj) {
                nsstring_slice slice(key);
                dict.writeKey(slice);
                dict.write([obj objectForKey: key]);
            }
        } else if ([obj isKindOfClass: [NSArray class]]) {
            encoder array = writeArray((uint32_t)[obj count]);
            for (NSString* item in obj) {
                array.write(item);
            }
        } else if ([obj isKindOfClass: [NSData class]]) {
            writeData(slice((NSData*)obj));
        } else if ([obj isKindOfClass: [NSNull class]]) {
            writeNull();
        } else {
            NSCAssert(NO, @"Objects of class %@ are not encodable", [obj class]);
        }
    }

}