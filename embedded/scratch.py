import struct
import usb1  # pip install libusb1

WRITE_SLOT_1 = 0x03
WRITE_SLOT_2 = 0x04

with open("nyan.gif", "rb") as f:
    gif_content = f.read()

with usb1.USBContext() as context:
    handle = context.openByVendorIDAndProductID(
        0xCafe,
        0x0420,
        skip_on_error=True,
    )
    handle.claimInterface(0)
    handle.controlWrite(2 << 5 | 1, WRITE_SLOT_1, 0, 0, bytes())

    out_buffer = struct.pack("<I", len(gif_content)) + gif_content
    CHUNK_SIZE = 256
    for i in range(0, len(out_buffer), CHUNK_SIZE):
        print(handle.bulkWrite(1, out_buffer[i: i + CHUNK_SIZE]))
