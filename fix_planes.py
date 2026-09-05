with open("Median/median.cpp", "r") as f:
    code = f.read()

import re
old_block = """  if (cs == 1) {
    ProcessPlane_T<uint8_t>(PLANAR_Y, src, dst, env);
    ProcessPlane_T<uint8_t>(PLANAR_U, src, dst, env);
    ProcessPlane_T<uint8_t>(PLANAR_V, src, dst, env);
  } else if (cs == 2) {
    ProcessPlane_T<uint16_t>(PLANAR_Y, src, dst, env);
    ProcessPlane_T<uint16_t>(PLANAR_U, src, dst, env);
    ProcessPlane_T<uint16_t>(PLANAR_V, src, dst, env);
  } else if (cs == 4) {
    ProcessPlane_T<float>(PLANAR_Y, src, dst, env);
    ProcessPlane_T<float>(PLANAR_U, src, dst, env);
    ProcessPlane_T<float>(PLANAR_V, src, dst, env);
  }"""

new_block = """  if (cs == 1) {
    ProcessPlane_T<uint8_t>(PLANAR_Y, src, dst, env);
    if (!info[0].IsY()) {
      ProcessPlane_T<uint8_t>(PLANAR_U, src, dst, env);
      ProcessPlane_T<uint8_t>(PLANAR_V, src, dst, env);
    }
  } else if (cs == 2) {
    ProcessPlane_T<uint16_t>(PLANAR_Y, src, dst, env);
    if (!info[0].IsY()) {
      ProcessPlane_T<uint16_t>(PLANAR_U, src, dst, env);
      ProcessPlane_T<uint16_t>(PLANAR_V, src, dst, env);
    }
  } else if (cs == 4) {
    ProcessPlane_T<float>(PLANAR_Y, src, dst, env);
    if (!info[0].IsY()) {
      ProcessPlane_T<float>(PLANAR_U, src, dst, env);
      ProcessPlane_T<float>(PLANAR_V, src, dst, env);
    }
  }"""

if old_block in code:
    code = code.replace(old_block, new_block)
else:
    print("Could not find block to replace.")

with open("Median/median.cpp", "w") as f:
    f.write(code)
