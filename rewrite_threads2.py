with open("Median/median.h", "r") as f:
    h_code = f.read()

h_code = h_code.replace(
    "void ProcessPlane_T(int plane, PVideoFrame src[MAX_DEPTH], PVideoFrame& dst);",
    "void ProcessPlane_T(int plane, PVideoFrame src[MAX_DEPTH], PVideoFrame& dst, IScriptEnvironment* env);"
)
h_code = h_code.replace(
    "void ProcessPlanarFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst);",
    "void ProcessPlanarFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst, IScriptEnvironment* env);"
)
h_code = h_code.replace(
    "void ProcessInterleavedFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst);",
    "void ProcessInterleavedFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst, IScriptEnvironment* env);"
)

h_code += "\nAVSValue __stdcall MedianWorker(IScriptEnvironment2* env, void* data);\n"

with open("Median/median.h", "w") as f:
    f.write(h_code)
