import sys
import usb1
import time

VENDOR_ID = 0x0eb7
DEVICE_ID = 0x0005

CMD_BASE = [0x01]
CMD_LED_BASE = [0xf8]
CMD_BASE_LED = [0x13] # CMD_BASE+CMD_LED_BASE+CMD_BASE_LED+[val]
CMD_WHEEL_LED = [0x09,0x08,0x00] # CMD_BASE+CMD_LED_BASE+CMD_WHEEL_LED+[val]

CMD_SIMPLE_SPRING_ENABLE = [0x11,0x0b]
CMD_SIMPLE_SPRING_DISABLE = [0x13,0x0b]
CMD_SIMPLE_SPRING_ARGS = [0x80,0x80,0x10,0x00,0x00] # offset?, offset?, coefficient, ??, saturation

CMD_FORCE_SPRING = [0x01,0x08] # CMD_BASE+CMD_FORCE_SPRING+[val] # val 0x80 -> no force; val 0x00 -> full force to the left
CMD_FORCE_SPRING_DISABLE = [0x03,0x08]

def payload(args):
    p = bytearray(CMD_BASE+args)
    # p.extend(args)
    # pad with zeros
    p.extend([0]*(8-len(p)))
    print(p)
    return p

if __name__ == '__main__':
    val = None
    if len(sys.argv)==2:
        val = int(sys.argv[1])

    with usb1.USBContext() as context:
        handle = context.openByVendorIDAndProductID(
            VENDOR_ID,
            DEVICE_ID,
            skip_on_error=True,
        )
        if handle is None:
            print('Device not found')
            sys.exit(1)

        handle.setAutoDetachKernelDriver(True)
        with handle.claimInterface(0):
            # request reading of current value
            handle.interruptWrite(0x01, payload(CMD_LED_BASE+CMD_WHEEL_LED+[val]))
            # handle.interruptWrite(0x01, payload(CMD_FORCE_SPRING+[val]))
            # time.sleep(1)
            # handle.interruptWrite(0x01, payload(CMD_FORCE_SPRING_DISABLE))
            # handle.interruptWrite(0x01, payload(CMD_SIMPLE_SPRING_ENABLE+CMD_SIMPLE_SPRING_ARGS))
            # time.sleep(5)
            # handle.interruptWrite(0x01, payload(CMD_SIMPLE_SPRING_DISABLE))

# Base LED test
# EP0x01, OUT
# URB transfer type: URB_INTERRUPT (0x01)
# Leftover Capture Data: 01f8130100000000
# Leftover Capture Data: 01f8130300000000
# Leftover Capture Data: 01f8130700000000
# Leftover Capture Data: 01f8130f00000000
# Leftover Capture Data: 01f8131f00000000
# Leftover Capture Data: 01f8133f00000000
# Leftover Capture Data: 01f8137f00000000
# Leftover Capture Data: 01f813ff00000000
# Leftover Capture Data: 01f813ff01000000
# Leftover Capture Data: 01f813ff00000000


# Wheel LED test
# Endpoint: 0x01, Direction: OUT
# URB transfer type: URB_INTERRUPT (0x01)
# Leftover Capture Data: 01f8130000000000
# Leftover Capture Data: 01f814ff00000000
# Leftover Capture Data: 01f8090201000000
# Leftover Capture Data: 01f8090801000000
# Leftover Capture Data: 01f8090801800000
# Leftover Capture Data: 01f8090801c00000
# Leftover Capture Data: 01f8090801e00000
# Leftover Capture Data: 01f8090801f00000
# Leftover Capture Data: 01f8090801f80000
# Leftover Capture Data: 01f8090801fc0000
# Leftover Capture Data: 01f8090801fe0000
# Leftover Capture Data: 01f8090801ff0000
# Leftover Capture Data: 01f8090801fe0000


# FFB test
# Endpoint: 0x01, Direction: OUT
# URB transfer type: URB_INTERRUPT (0x01)
# Leftover Capture Data: 0000000000000000 # start 
# Leftover Capture Data: 0100000000000000
# Leftover Capture Data: 0103080000000000
# Leftover Capture Data: 01030b0000000000
# Leftover Capture Data: 01030c0000000000
# Leftover Capture Data: 0113080000000000
# Leftover Capture Data: 01130b0000000000
# Leftover Capture Data: 01130c0000000000
# Leftover Capture Data: 0123080000000000
# Leftover Capture Data: 01230b0000000000
# Leftover Capture Data: 01230c0000000000
# Leftover Capture Data: 0133080000000000
# Leftover Capture Data: 01330b0000000000
# Leftover Capture Data: 01330c0000000000
# Leftover Capture Data: 0143080000000000
# Leftover Capture Data: 01430b0000000000
# Leftover Capture Data: 01430c0000000000
# Leftover Capture Data: 0153080000000000
# Leftover Capture Data: 01530b0000000000
# Leftover Capture Data: 01530c0000000000
# Leftover Capture Data: 0163080000000000
# Leftover Capture Data: 01630b0000000000
# Leftover Capture Data: 01630c0000000000
# Leftover Capture Data: 0173080000000000
# Leftover Capture Data: 01730b0000000000
# Leftover Capture Data: 01730c0000000000
# Leftover Capture Data: 0183080000000000
# Leftover Capture Data: 01830b0000000000
# Leftover Capture Data: 01830c0000000000
# Leftover Capture Data: 0193080000000000
# Leftover Capture Data: 01930b0000000000
# Leftover Capture Data: 01930c0000000000
# Leftover Capture Data: 01a3080000000000
# Leftover Capture Data: 01a30b0000000000
# Leftover Capture Data: 01a30c0000000000
# Leftover Capture Data: 01b3080000000000
# Leftover Capture Data: 01b30b0000000000
# Leftover Capture Data: 01b30c0000000000
# Leftover Capture Data: 01c3080000000000
# Leftover Capture Data: 01c30b0000000000
# Leftover Capture Data: 01c30c0000000000
# ...
# Leftover Capture Data: 01f3080000000000
# Leftover Capture Data: 01f30b0000000000
# Leftover Capture Data: 01f30c0000000000 # etwa 200ms
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# ...
# Leftover Capture Data: 010108ff00000000 # etwa 2s
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 0103080000000000


### Wheel Tester
# Spring Force: Simple Spring -> spring effect
# offset x=0 y=0
# saturation x=10000 y=10000
# coefficient 10000
# Leftover Capture Data: 01110b8080ff00ff
# Disabled
# Leftover Capture Data: 01130b0000000000

# Spring Force: Simple Spring -> spring effect
# offset x=-9999 y=9999
# saturation x=10000 y=10000
# coefficient 10000
# Leftover Capture Data: 01110b8080ff00ff
# Leftover Capture Data: 01110b0000ff00ff
# Disabled
# Leftover Capture Data: 01130b0000000000

# Spring Force: Simple Spring -> spring effect
# offset x=0 y=0
# saturation x=10000 y=10000
# coefficient 5000
# Leftover Capture Data: 01110b0000ff00ff
# Leftover Capture Data: 01110b80808800ff
# Disabled
# Leftover Capture Data: 01130b0000000000

# Spring Force: Simple Spring -> spring effect
# offset x=0 y=0
# saturation x=5000 y=5000
# coefficient 10000
# Leftover Capture Data: 01110b80808800ff
# Leftover Capture Data: 01110b8080ff0080
# Disabled
# Leftover Capture Data: 01130b0000000000

# Spring Force: Simple Spring -> spring effect
# offset x=0 y=0
# saturation x=10000 y=10000
# coefficient 10000
# Leftover Capture Data: 01110b8080ff0080
# Leftover Capture Data: 01110b8080ff00ff
# Disabled
# Leftover Capture Data: 01130b0000000000

# Spring Force: Simple Spring -> spring effect
# offset x=-3000 y=2000
# saturation x=500 y=600
# coefficient 7000
# Leftover Capture Data: 01110b8080ff00ff
# Leftover Capture Data: 01110b595911000d
# Disabled
# Leftover Capture Data: 01130b0000000000



# Spring Force: Force Spring -> spring effect
# offset x=0 y=0
# saturation x=10000 y=10000
# coefficient 10000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Disabled
# Leftover Capture Data: 0103080000000000


# Spring Force: Force Spring -> spring effect
# offset x=0 y=0
# saturation x=10000 y=10000
# coefficient 2000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Disabled
# Leftover Capture Data: 0103080000000000

# Spring Force: Force Spring -> spring effect
# offset x=768 y=-768
# saturation x=10000 y=10000
# coefficient 2000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 010108ff00000000


# Spring Force: Amplitude Sweep -> constant effect ?
# offset x=0 y=0
# saturation x=10000 y=10000
# coefficient 10000
# Leftover Capture Data: 0101087f00000000
# Leftover Capture Data: 0101087c00000000
# Leftover Capture Data: 0101087800000000
# Leftover Capture Data: 0101087400000000
# Leftover Capture Data: 0101087000000000
# Leftover Capture Data: 0101086c00000000
# Leftover Capture Data: 0101086900000000
# Leftover Capture Data: 0101086700000000
# Leftover Capture Data: 010108ec00000000
# Leftover Capture Data: 010108ed00000000
# Leftover Capture Data: 010108ef00000000
# Leftover Capture Data: 010108ea00000000



# Spring Force: Amplitude Sweep -> constant effect ?
# offset x=768 y=-768
# saturation x=10000 y=10000
# coefficient 2000
# Leftover Capture Data: 0101080100000000
# Leftover Capture Data: 0101087c00000000
# Leftover Capture Data: 0101087800000000
# Leftover Capture Data: 0101089a00000000
# Leftover Capture Data: 010108a200000000
# Leftover Capture Data: 010108a900000000
# Leftover Capture Data: 010108a600000000
# Leftover Capture Data: 010108a100000000
# Leftover Capture Data: 0101089b00000000




# Wheel angle/sensivity 801
# Leftover Capture Data: 01f8812103000000
# Leftover Capture Data: 0000000000000000
# URB_INTERRUPT in
# Leftover Capture Data: 01f5000000000000
# Leftover Capture Data: 01f8090106010000
# Leftover Capture Data: 01f8812103000000
# URB_INTERRUPT in
# Leftover Capture Data: 0100000000000000
# Leftover Capture Data: 0000000000000000
# Leftover Capture Data: 01f5000000000000
# Leftover Capture Data: 01f8090106010000
# Leftover Capture Data: 01f8812103000000
# URB_INTERRUPT in
# Leftover Capture Data: 0100000000000000


# Wheel angle/sensivity 1080
# Leftover Capture Data: 01f8813804000000
# Leftover Capture Data: 0000000000000000
# URB_INTERRUPT in
# Leftover Capture Data: 01f5000000000000
# Leftover Capture Data: 01f8090106010000
# Leftover Capture Data: 01f8813804000000
# URB_INTERRUPT in
# Leftover Capture Data: 0100000000000000
# Leftover Capture Data: 0000000000000000
# Leftover Capture Data: 01f5000000000000
# Leftover Capture Data: 01f8090106010000
# Leftover Capture Data: 01f8813804000000
# URB_INTERRUPT in
# Leftover Capture Data: 0100000000000000



