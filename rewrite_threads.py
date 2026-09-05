import re

with open("Median/median.h", "r") as f:
    h_code = f.read()

# Add struct for thread parameters
struct_code = """
struct MedianJobData {
  const Median* filter;
  int plane;
  PVideoFrame* src;
  PVideoFrame* dst;
  int start_y;
  int end_y;
  int component_size;
  bool is_interleaved;
};
"""
h_code = h_code.replace("class Median : public GenericVideoFilter\n{", struct_code + "\nclass Median : public GenericVideoFilter\n{")

with open("Median/median.h", "w") as f:
    f.write(h_code)
