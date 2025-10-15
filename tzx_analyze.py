import struct

with open(r'data\All Stars Tape 2 - Side B.tzx', 'rb') as f:
    f.read(10)
    block_num = 0
    print('TZX Blocks:')
    print('='*60)
    
    while True:
        id_byte = f.read(1)
        if not id_byte: break
        block_id = id_byte[0]
        
        if block_id == 0x10:
            pause = struct.unpack('<H', f.read(2))[0]
            length = struct.unpack('<H', f.read(2))[0]
            flag = f.read(1)[0]
            block_type = 'Data'
            block_name = ''
            if flag == 0x00 and length == 19:
                data_type = f.read(1)[0]
                name_bytes = f.read(10)
                block_name = name_bytes.decode('ascii', errors='ignore').strip()
                f.read(length - 12)
                block_type = ['Program','Number array','Char array','Bytes'][data_type] if data_type < 4 else 'Unknown'
            else:
                f.read(length - 1)
            block_num += 1
            print(f'{block_num}. Type={block_type:15s} Name="{block_name}"')
        elif block_id == 0x11:
            f.read(15)
            length = struct.unpack('<I', (f.read(3) + b'\x00'))[0]
            f.read(length)
            block_num += 1
            print(f'{block_num}. Type=Turbo')
        elif block_id == 0x14:
            f.read(7)
            length = struct.unpack('<I', (f.read(3) + b'\x00'))[0]
            f.read(length)
            block_num += 1
            print(f'{block_num}. Type=Pure Data')
        elif block_id == 0x20: f.read(2)
        elif block_id == 0x12: f.read(2)
        elif block_id == 0x13: f.read(f.read(1)[0] * 2)
        elif block_id == 0x21: print(f'--- Group: {f.read(f.read(1)[0]).decode("ascii",errors="ignore")}')
        elif block_id == 0x30: print(f'--- Text: {f.read(f.read(1)[0]).decode("ascii",errors="ignore")}')
        elif block_id == 0x22: pass
        elif block_id == 0x31: f.read(1)
        elif block_id == 0x32: f.read(struct.unpack('<H', f.read(2))[0])
        elif block_id == 0x33: f.read(f.read(1)[0] * 3)
        else: break
    print('='*60)
    print(f'Total: {block_num} blocks')
