"""Adjust reviewed Korean image labels without changing fonts or draw geometry.

Only private, fingerprint-verified patch members are passed here by prepare_korean.
The resource reader is also used to audit every sprite, including maps and loading
screens. No game pixels or user logs are included in this module.
"""
import struct


class SpritePackage:
    def __init__(self, data):
        if len(data) < 20 or data[:4] != b'XPR0':
            raise ValueError('Invalid sprite package')
        self.data = data
        self.start = struct.unpack_from('<I', data, 8)[0]
        definitions = struct.unpack_from('<I', data, 16)[0]
        if definitions % 24 or not 20 + definitions <= self.start <= len(data):
            raise ValueError('Invalid sprite table')
        self.count = definitions // 24
        self.textures = []
        at = 20 + definitions
        while at + 4 <= self.start:
            common = struct.unpack_from('<I', data, at)[0]
            if common == 0xFFFFFFFF:
                break
            if common != 0x40001 or at + 20 > self.start:
                raise ValueError('Unexpected sprite resource')
            _, offset, _, fmt, _ = struct.unpack_from('<5I', data, at)
            width, height = 1 << (fmt >> 20 & 15), 1 << (fmt >> 24 & 15)
            kind = fmt >> 8 & 255
            at += 20
            palette = None
            if kind == 11:
                if at + 12 > self.start:
                    raise ValueError('Missing sprite palette')
                pc, po, _ = struct.unpack_from('<3I', data, at)
                at += 12
                if pc >> 16 & 7 != 3 or self.start + po + 1024 > len(data):
                    raise ValueError('Invalid sprite palette')
                palette = struct.unpack_from('<256I', data, self.start + po)
            if kind not in (6, 11, 25) or max(width, height) > 1024:
                raise ValueError('Unsupported sprite texture')
            size = 4 if kind == 6 else 1
            offset += self.start
            if offset + width * height * size > len(data):
                raise ValueError('Sprite pixels exceed package')
            self.textures.append((width, height, kind, offset, palette))

    def definition(self, index, allow_overflow=False):
        if not 0 <= index < self.count:
            raise ValueError('Sprite index exceeds table')
        u0, v0, u1, v1, texture, size = struct.unpack_from('<4f2I', self.data, 20 + index * 24)
        if texture >= len(self.textures):
            raise ValueError('Sprite texture index exceeds table')
        w, h, *_ = self.textures[texture]
        box = tuple(round(x) for x in (u0*w, v0*h, u1*w, v1*h))
        if not allow_overflow and (not 0 <= box[0] < box[2] <= w or not 0 <= box[1] < box[3] <= h):
            raise ValueError('Sprite window exceeds texture')
        return texture, box, size

    @staticmethod
    def swizzled(x, y, width, height):
        result, bit, shift = 0, 1, 0
        while bit < max(width, height):
            if bit < width:
                result |= bool(x & bit) << shift
                shift += 1
            if bit < height:
                result |= bool(y & bit) << shift
                shift += 1
            bit <<= 1
        return result

    def pixel(self, texture, x, y):
        w, h, kind, offset, palette = self.textures[texture]
        at = self.swizzled(x, y, w, h)
        if kind == 6:
            return struct.unpack_from('<I', self.data, offset + at * 4)[0]
        value = self.data[offset + at]
        return palette[value] if kind == 11 else value << 24 | 0xFFFFFF

    def ink_bounds(self, index):
        texture, (x0, y0, x1, y1), _ = self.definition(index)
        xs, ys = [], []
        for y in range(y0, y1):
            for x in range(x0, x1):
                color = self.pixel(texture, x, y)
                if color >> 24 and max(color >> s & 255 for s in (0, 8, 16)) > 120:
                    xs.append(x); ys.append(y)
        return (min(xs), min(ys), max(xs)+1, max(ys)+1) if xs else None

    def shift_pixels(self, index, dx):
        texture, (x0, y0, x1, y1), _ = self.definition(index)
        w, h, kind, offset, palette = self.textures[texture]
        size = 4 if kind == 6 else 1
        if kind == 11:
            transparent = next((i for i, c in enumerate(palette) if not c >> 24), None)
            if transparent is None:
                raise ValueError('No transparent palette entry')
            blank = bytes([transparent])
        else:
            blank = bytes(size)
        output = bytearray(self.data)
        for y in range(y0, y1):
            for x in range(x0, x1):
                sx = x - dx
                dst = offset + self.swizzled(x, y, w, h) * size
                if x0 <= sx < x1:
                    src = offset + self.swizzled(sx, y, w, h) * size
                    output[dst:dst+size] = self.data[src:src+size]
                else:
                    output[dst:dst+size] = blank
                # Reject the move if even a faint shadow would be lost.
                if not x0 <= x + dx < x1 and self.pixel(texture, x, y) >> 24:
                    raise ValueError('Image alignment would clip a visible pixel')
        return bytes(output)


# The original drawers center these heading quads. Translated lettering was
# centered in the full texture, while these original windows crop it off center.
# Tuples: sprite, texture width, original window width, translated ink center.
HEADING_WINDOWS = {
    'spr.afs/saveload.spr': ((1, 256, 218, 128.5), (2, 256, 254, 128)),
    'spr.afs/btl_menu.spr': ((0, 256, 202, 128.5),),
    'spr.afs/mis_menu.spr': ((0, 512, 410, 256.5),),
    'spr.afs/arcade.spr': ((2, 512, 480, 256),),
}


def correct_sprite_layout(member, data):
    if member not in HEADING_WINDOWS and member not in (
            'spr.afs/option.spr', 'spr.afs/gameover.spr', 'spr.afs/gameovr2.spr'):
        return data
    package = SpritePackage(data)
    output = bytearray(data)
    for index, width, end, center in HEADING_WINDOWS.get(member, ()):
        at = 20 + index * 24
        u0, v0, u1, v1, texture, size = struct.unpack_from('<4f2I', data, at)
        actual_width = package.textures[texture][0]
        if (u0, v0, u1, v1, actual_width) != (0, 0, end/width, 1, width):
            raise ValueError('Reviewed Korean heading layout changed')
        shift = (center - end/2) / width
        if not 0 <= u0 + shift < u1 + shift <= 1:
            raise ValueError('Heading alignment exceeds texture')
        struct.pack_into('<f', output, at, u0 + shift)
        struct.pack_into('<f', output, at + 8, u1 + shift)
    if member == 'spr.afs/option.spr':
        if package.count != 62:
            raise ValueError('Reviewed Korean Options layout changed')
        # Device quads: label x=58.5,width=160; button center=160. The label
        # ink is centered at x=138.5. Move the existing pixels 22 console
        # pixels right (within half a pixel of the button), without resampling.
        for index in range(13, 19):
            package = SpritePackage(bytes(output))
            texture, box, _ = package.definition(index)
            if texture != 11 or box != ((index-13)*160, 0, (index-12)*160, 32):
                raise ValueError('Reviewed Korean Options label bounds changed')
            output = bytearray(package.shift_pixels(index, 22))
    elif member in ('spr.afs/gameover.spr', 'spr.afs/gameovr2.spr'):
        if package.count != 29 or package.definition(0)[:2] != (0, (0, 0, 256, 64)):
            raise ValueError('Reviewed Korean Game Over layout changed')
        # The sharp lettering is 17.5 pixels right of its quad's center.
        # The separate blurred effect is already centered; leave it intact.
        output = bytearray(package.shift_pixels(0, -18))
    return bytes(output)
