import re

with open("Median/median.cpp", "r") as f:
    code = f.read()

# Update constructor
code = code.replace(
    "Median::Median(PClip _child, std::vector<PClip> _clips, unsigned int _low, unsigned int _high, bool _temporal, bool _processchroma, unsigned int _sync, unsigned int _samples, bool _debug, IScriptEnvironment* env) :",
    "Median::Median(PClip _child, std::vector<PClip> _clips, unsigned int _low, unsigned int _high, bool _temporal, bool _processchroma, unsigned int _sync, unsigned int _samples, bool _debug, unsigned int _threads, IScriptEnvironment* env) :"
)
code = code.replace(
    "GenericVideoFilter(_child), clips(_clips), low(_low), high(_high), temporal(_temporal), processchroma(_processchroma), sync(_sync), samples(_samples), debug(_debug)",
    "GenericVideoFilter(_child), clips(_clips), low(_low), high(_high), temporal(_temporal), processchroma(_processchroma), sync(_sync), samples(_samples), debug(_debug), threads(std::max(1U, _threads))"
)

# Add #include <thread> and <algorithm> at top
code = code.replace("#include <vector>", "#include <vector>\n#include <thread>\n#include <algorithm>")

with open("Median/median.cpp", "w") as f:
    f.write(code)
