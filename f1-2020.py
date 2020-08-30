import socket

sysfs_base = "/sys/module/hid_fanatecff/drivers/hid:ftec_csl_elite/0003:0EB7:0005.00C3"
sysfs_rpm = "%s/leds/0003:0EB7:0005.00C3::RPM"%sysfs_base

# clear leds and display
def clear():
    for i in range(9):
        open('%s%i/brightness'%(sysfs_rpm,i+1),'w').write("0")
    open('%s/display'%sysfs_base,'w').write("-1")

UDP_IP = "127.0.0.1"
UDP_PORT = 20777

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

clear()

try:
    while True:
        data, addr = sock.recvfrom(2048)
        pkt_id = data[5]
        # only use telemetry packets
        if pkt_id != 6: continue
        car_idx = data[22]

        # start car delemetry data byte 24
        # size of car delemetry data 58
        car_telem_start = 24 + car_idx*58
        speed = int.from_bytes(data[car_telem_start:car_telem_start+2], byteorder="little")
        gear = int(data[car_telem_start+15])
        rpm = int.from_bytes(data[car_telem_start+16:car_telem_start+18], byteorder="little")
        suggestedGear = int(data[-1])

        for i in range(9):
            open('%s%i/brightness'%(sysfs_rpm,i+1),'w').write("0" if (rpm-5000)<i*1000 else "1")

        open('%s/display'%sysfs_base,'w').write("%s"%speed)
        # print(speed, rpm, gear)
        
except (KeyboardInterrupt, SystemExit):
    print("Exiting")
except Exception as e:
    raise e
finally:
    clear()