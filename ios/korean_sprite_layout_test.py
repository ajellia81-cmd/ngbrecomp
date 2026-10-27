"""Synthetic sprites and measured device coordinates; no game data."""
import collections
import struct
import unittest
from korean_sprite_layout import SpritePackage, HEADING_WINDOWS, correct_sprite_layout


def fixture(count, width, height, definitions, textures=1):
    start = 4096
    b = bytearray(start + width*height + 1024)
    b[:4] = b'XPR0'
    struct.pack_into('<I', b, 8, start)
    struct.pack_into('<I', b, 16, count*24)
    for index, texture, box in definitions:
        x0,y0,x1,y1 = box
        struct.pack_into('<4f2I', b, 20+24*index, x0/width,y0/height,x1/width,y1/height,
                         texture, width | height << 16)
    fmt = 11 << 8 | (width.bit_length()-1) << 20 | (height.bit_length()-1) << 24
    for i in range(textures):
        at = 20 + count*24 + i*32
        struct.pack_into('<5I', b, at, 0x40001,0,0,fmt,0)
        struct.pack_into('<3I', b, at+20, 0x830001,width*height,0)
    struct.pack_into('<I', b, 20+count*24+textures*32, 0xFFFFFFFF)
    struct.pack_into('<3I', b, start+width*height, 0,0xFFFFFFFF,0x01FFFFFF)
    return b


def put(b, x, y, value):
    p = SpritePackage(b);w,h,_,offset,_ = p.textures[0]
    b[offset+p.swizzled(x,y,w,h)] = value


def visible(p, texture, box):
    x0,y0,x1,y1 = box
    return collections.Counter(p.pixel(texture,x,y) for y in range(y0,y1)
                               for x in range(x0,x1) if p.pixel(texture,x,y) >> 24)


class KoreanImageLayout(unittest.TestCase):
    def test_heading_windows_keep_extent_geometry_and_pixels(self):
        for name, specs in HEADING_WINDOWS.items():
            width=specs[0][1]
            b=fixture(max(s[0] for s in specs)+1,width,32,
                      [(i,0,(0,0,end,32)) for i,_,end,_ in specs])
            fixed=correct_sprite_layout(name,bytes(b))
            allowed=set()
            for index,w,end,center in specs:
                at=20+index*24
                u0,v0,u1,v1,_,_=struct.unpack_from('<4f2I',fixed,at)
                self.assertAlmostEqual((u0+u1)*w/2,center)
                self.assertAlmostEqual((u1-u0)*w,end)
                self.assertEqual((v0,v1),(0,1))
                allowed.update(at+j for j in (*range(4),*range(8,12)))
            self.assertTrue(all(a==c or i in allowed for i,(a,c) in enumerate(zip(b,fixed))))

    def test_options_replay_centers_all_rows_and_preserves_every_visible_pixel(self):
        b=fixture(62,1024,32,[(i,11,((i-13)*160,0,(i-12)*160,32))
                              for i in range(13,19)],12)
        for row in range(6):
            for y in range(2,22):
                for x in range(36,124):put(b,row*160+x,y,1)
            put(b,row*160+35,22,2) # Even a nearly transparent shadow survives.
        before=SpritePackage(bytes(b))
        fixed=correct_sprite_layout('spr.afs/option.spr',bytes(b));after=SpritePackage(fixed)
        self.assertEqual(fixed[:4096],bytes(b[:4096]))
        for index in range(13,19):
            texture,box,_=before.definition(index)
            self.assertEqual(visible(before,texture,box),visible(after,texture,box))
            bounds=after.ink_bounds(index)
            ink_center=(bounds[0]+bounds[2])/2-box[0]
            # Recorded original quad: label begins at 58.5, button spans
            # 56.5..263.5. Verify the same coordinates across viewport scales.
            for aspect in (4/3,16/9,16/10,21/9):
                scale=480*aspect/640
                self.assertLessEqual(abs((58.5+ink_center-160)*scale),.5*scale)
        # No edits outside the six label cells, including texture palettes.
        self.assertEqual(visible(before,11,(960,0,1024,32)),visible(after,11,(960,0,1024,32)))
        self.assertEqual(fixed[-1024:],bytes(b[-1024:]))

    def test_game_over_changes_only_sharp_text_and_does_not_drop_shadows(self):
        b=fixture(29,256,64,[(0,0,(0,0,256,64))])
        for x in range(61,230):put(b,x,20,1)
        put(b,60,21,2)
        before=SpritePackage(bytes(b));fixed=correct_sprite_layout('spr.afs/gameover.spr',bytes(b))
        after=SpritePackage(fixed)
        self.assertEqual(visible(before,0,(0,0,256,64)),visible(after,0,(0,0,256,64)))
        self.assertEqual(fixed[:4096],bytes(b[:4096]))
        put(b,4,20,2)
        with self.assertRaisesRegex(ValueError,'clip'):correct_sprite_layout('spr.afs/gameover.spr',bytes(b))

    def test_maps_art_and_changed_layout_are_not_silently_modified(self):
        data=b'not a sprite: untouched map, picture, or other language'
        self.assertIs(correct_sprite_layout('spr.afs/ys_1f.spr',data),data)
        b=fixture(3,256,32,[(1,0,(0,0,217,32)),(2,0,(0,0,254,32))])
        with self.assertRaises(ValueError):correct_sprite_layout('spr.afs/saveload.spr',bytes(b))


if __name__ == '__main__':unittest.main()
