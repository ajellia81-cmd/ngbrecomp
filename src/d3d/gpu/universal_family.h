#pragma once
#include <cstdint>
namespace gpu {
// A full opaque write has no destination dependency. Masked or blended writes
// still use the ordered attachment-read shader, including the water path.
template<class Key> bool UniversalReadsColor(const Key& k){return k.has_rt && (k.blend || k.wmask!=15);}
template<class Key> uint32_t UniversalFamily(const Key& k){
 return (k.samples?k.samples:1)<<8 | uint32_t(UniversalReadsColor(k))<<7 |
  (k.has_rt?1:0)<<6 | (k.has_ds?1:0)<<5 |
  (k.topo==2?(k.fill==0x1B01?1:k.fill==0x1B00?2:0):0)<<2 | k.topo;
}
}
