#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys

translate_map = {
    'а': 'a', 'б': 'b', 'в': 'B', 'г': 'g', 'д': 'd',
    'е': 'E', 'ё': 'E', 'ж': 'v', 'з': 'z', 'и': 'i',
    'й': 'j', 'к': 'k', 'л': 'l', 'м': 'm', 'н': 'H',
    'о': 'o', 'п': 'p', 'р': 'P', 'с': 'C', 'т': 't',
    'у': 'u', 'ф': 'f', 'х': 'X', 'ц': 'c', 'ч': 'h',
    'ш': '{', 'щ': '}', 'ъ': '\'', 'ы': 'y', 'ь': 'x',
    'э': 'e', 'ю': 'w', 'я': 'q',
    'А': 'a', 'Б': 'b', 'В': 'B', 'Г': 'g', 'Д': 'd',
    'Е': 'E', 'Ё': 'E', 'Ж': 'v', 'З': 'z', 'И': 'i',
    'Й': 'j', 'К': 'k', 'Л': 'l', 'М': 'm', 'Н': 'H',
    'О': 'o', 'П': 'p', 'Р': 'P', 'С': 'C', 'Т': 't',
    'У': 'u', 'Ф': 'f', 'Х': 'X', 'Ц': 'c', 'Ч': 'h',
    'Ш': '{', 'Щ': '}', 'Ъ': '\'', 'Ы': 'y', 'Ь': 'x',
    'Э': 'e', 'Ю': 'w', 'Я': 'q'
}


def chunks(lst, n):
    for i in range(0, len(lst), n):
        yield lst[i:i + n]


def hex2chr(hex: str) -> chr:
    return chr(int(hex, 16))


def int2hex(value: int, byteorder: str) -> bytes:
    return value.to_bytes(2, byteorder)


def translate_line(line: str) -> bytes:
    new_line = line[:-1] if line[-1] == '\x0A' else line

    while '@' in new_line:
        pos = new_line.find('@')
        col1 = hex2chr(new_line[pos+1:pos+3])
        col2 = hex2chr(new_line[pos+3:pos+5])
        subs = new_line[pos:pos+5]
        new_line = new_line.replace(subs, f'\xFF{col1}{col2}')

    for old, new in translate_map.items():
        new_line = new_line.replace(old, new)
    
    return (new_line + "\x00").encode("latin-1")
    

if __name__ == "__main__":
    clean_offsets = [0]
    lines = []

    with open(sys.argv[1], 'r') as input:
        for line in input.readlines():
            translated = translate_line(line)
            lines.append(translated)
            clean_offsets.append(clean_offsets[-1] + len(translated))

    output_file = sys.argv[2]
    is_tbn = output_file.upper().endswith('.TBN')
    
    offsets_shift = (len(clean_offsets) - 1) * 2 if is_tbn else 0
    offsets = [int2hex(
            offset + offsets_shift,
            'little' if is_tbn else 'big'
        ) for offset in clean_offsets]

    if is_tbn:
        with open(output_file, 'wb') as output:
            for offset in offsets[:-1]:
                output.write(offset)
            for line in lines:
                output.write(line.replace(b'|', b'\x0A').replace(b'#', b'\x0B'))

        byte_list = []
        with open(sys.argv[2], "rb") as file:
            byte = file.read(1)
            while byte:
                byte_list.append(byte.hex().upper())
                byte = file.read(1)

        for row in list(chunks([f"0x{b}" for b in byte_list], 16)):
            print(", ".join(row), end=",\n")

    elif output_file.upper().endswith('.BIN'):
        with open(output_file, 'wb') as offset_output:
            for offset in offsets[:-1]:
                offset_output.write(offset)
        
        with open(output_file.replace('.BIN', '.TXT'), 'wb') as output:
            for line in lines:
                output.write(line.replace(b'\x00', b'\x0A'))
    
    else:
        print('Unknown output format')
