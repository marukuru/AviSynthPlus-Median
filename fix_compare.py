with open("Median/median.cpp", "r") as f:
    code = f.read()

import re

old_compare = r"double Median::CompareFrames\(int plane, PVideoFrame a, PVideoFrame b, unsigned int points\)\s*\{\s*const unsigned char\* aptr = a->GetReadPtr\(plane\);.*?return 100\.0 - difference;\s*\}"

new_compare = """double Median::CompareFrames(int plane, PVideoFrame a, PVideoFrame b, unsigned int points)
{
  int cs = info[0].ComponentSize();
  const unsigned int length = a->GetRowSize(plane) / cs * a->GetHeight(plane);
  if (points < 1 || points > length) points = length;
  const unsigned int step = length / points;

  double difference = 0.0;
  
  if (cs == 1) {
    const uint8_t* aptr = (const uint8_t*)a->GetReadPtr(plane);
    const uint8_t* bptr = (const uint8_t*)b->GetReadPtr(plane);
    unsigned long sum = 0;
    for (unsigned int i = 0; i < length; i += step) sum += abs((int)aptr[i] - (int)bptr[i]);
    difference = (100.0 * sum) / (255.0 * points);
  } else if (cs == 2) {
    const uint16_t* aptr = (const uint16_t*)a->GetReadPtr(plane);
    const uint16_t* bptr = (const uint16_t*)b->GetReadPtr(plane);
    unsigned long long sum = 0;
    for (unsigned int i = 0; i < length; i += step) sum += abs((int)aptr[i] - (int)bptr[i]);
    difference = (100.0 * sum) / (65535.0 * points);
  } else if (cs == 4) {
    const float* aptr = (const float*)a->GetReadPtr(plane);
    const float* bptr = (const float*)b->GetReadPtr(plane);
    double sum = 0.0;
    for (unsigned int i = 0; i < length; i += step) sum += std::abs(aptr[i] - bptr[i]);
    difference = (100.0 * sum) / (1.0 * points);
  }

  return 100.0 - difference;
}"""

match = re.search(old_compare, code, re.DOTALL)
if match:
    code = code[:match.start()] + new_compare + code[match.end():]
else:
    print("Could not find CompareFrames.")

with open("Median/median.cpp", "w") as f:
    f.write(code)
